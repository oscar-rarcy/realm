#include "realm.h"
#include <cstdio>
#include <map>
#ifdef _WIN32
// Winsock port (MinGW/MSYS2). Socket handles are stored in plain ints like
// the POSIX build — Windows handles are small in practice; CI compiles this,
// and the first Windows playtest is the runtime proof.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/time.h>            // MinGW provides gettimeofday
static int  netErrno()      { return WSAGetLastError(); }
static bool errWouldBlock() { int e = WSAGetLastError(); return e == WSAEWOULDBLOCK; }
static bool errInProgress() { int e = WSAGetLastError(); return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
static void closeRawSock(int fd) { closesocket(fd); }
static void netPlatformInit() {
    static bool done = false;
    if (!done) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); done = true; }
}
// Winsock buffer parameters are char*, not void* — cast in one place.
static ssize_t nsend(int s, const void* b, size_t n)  { return send(s, (const char*)b, (int)n, 0); }
static ssize_t nrecv(int s, void* b, size_t n)        { return recv(s, (char*)b, (int)n, 0); }
static int nsetsockopt(int s, int l, int o, const void* v, socklen_t n) { return setsockopt(s, l, o, (const char*)v, n); }
static int ngetsockopt(int s, int l, int o, void* v, socklen_t* n)      { return getsockopt(s, l, o, (char*)v, n); }
static ssize_t nsendto(int s, const void* b, size_t n, const sockaddr* a, socklen_t al)
    { return sendto(s, (const char*)b, (int)n, 0, a, al); }
static ssize_t nrecvfrom(int s, void* b, size_t n, sockaddr* a, socklen_t* al)
    { return recvfrom(s, (char*)b, (int)n, 0, a, al); }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#ifndef __EMSCRIPTEN__
#include <ifaddrs.h>   // no interface enumeration in the browser sandbox
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
static int  netErrno()      { return errno; }
static bool errWouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
static bool errInProgress() { return errno == EINPROGRESS; }
static void closeRawSock(int fd) { close(fd); }
static void netPlatformInit() {
    // A send() to a peer that already closed raises SIGPIPE, whose default
    // action kills the process silently (no crash report, buffered stdout
    // lost). Ignore it so the send fails with EPIPE instead — every send
    // path already treats an error as "peer gone". Killed the host of an
    // 8-seat match when the seven clients closed at staggered times.
    static bool done = false;
    if (!done) { signal(SIGPIPE, SIG_IGN); done = true; }
}
static ssize_t nsend(int s, const void* b, size_t n)  { return send(s, b, n, 0); }
static ssize_t nrecv(int s, void* b, size_t n)        { return recv(s, b, n, 0); }
static int nsetsockopt(int s, int l, int o, const void* v, socklen_t n) { return setsockopt(s, l, o, v, n); }
static int ngetsockopt(int s, int l, int o, void* v, socklen_t* n)      { return getsockopt(s, l, o, v, n); }
static ssize_t nsendto(int s, const void* b, size_t n, const sockaddr* a, socklen_t al)
    { return sendto(s, b, n, 0, a, al); }
static ssize_t nrecvfrom(int s, void* b, size_t n, sockaddr* a, socklen_t* al)
    { return recvfrom(s, b, n, 0, a, al); }
#endif

// ============================================================
// LOCKSTEP NETWORKING (docs/networking-plan.md)
//
// Nothing but Commands ever crosses the wire: every machine runs the whole
// deterministic sim (AIs included) from a shared seed, and a command issued
// at tick T executes EVERYWHERE at T+NET_CMD_DELAY.
//
// Topology is a HOST-CENTRED STAR (up to 1 + MAX_NET_CLIENTS humans):
//   client -> host   MSG_BUNDLE   that client's own commands for tick E
//   host   -> all    MSG_TICKSET  ALL seats' commands for E, merged in seat
//                                 order — one canonical stream, so arrival
//                                 order can never desync anyone.
// The host may only merge E once every live client's bundle for E is in;
// a client may only run E once TICKSET(E) arrives. TCP (or the WebSocket
// relay) keeps each leg ordered and reliable. Same-binary-same-arch is a
// documented constraint (floats in the sim, host-endian ints on the wire) —
// matching NET_PROTO_VERSION is required at the handshake.
// ============================================================

static const unsigned NET_PROTO_VERSION = 7;   // bump on ANY wire or sim-format change (v7: 8-player lobbies)

// REALM_NET_TRACE=1: log every frame in/out (harness debugging).
static bool netTrace() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("REALM_NET_TRACE"); on = (e && *e && *e != '0') ? 1 : 0; }
    return on;
}

// Frame: u32 payload length, u8 type, payload bytes.
enum : unsigned char {
    MSG_HELLO   = 'H',   // client->host  u32 proto, char name[24]
    MSG_WELCOME = 'W',   // host->client  u32 proto, char name[24]
    MSG_CONFIG  = 'C',   // host->client  NetMatchConfig (u64 + 10 x i32)
    MSG_ROSTER  = 'R',   // host->client  u8 yourSeat, char names[MAX_PLAYERS][24]
    MSG_START   = 'S',   // host->client  begin the match
    MSG_BUNDLE  = 'B',   // client->host  i32 execTick, i32 nCmds, OWN commands
    MSG_TICKSET = 'M',   // host->client  i32 execTick, i32 nCmds, ALL commands (seat order)
    MSG_HASH    = 'A',   // client->host  i32 tick, u64 simStateHash
    MSG_DESYNC  = 'D',   // host->client  i32 tick — a seat's hash disagreed
    MSG_DROP    = 'X',   // host->client  u8 seat, char name[24] — left mid-match
    MSG_PAUSE   = 'P',   // client->host  u8 paused | host->client u8 pausedMask
    MSG_CHAT    = 'T',   // both ways     u8 seat, utf-8 text (control channel, never sim)
    MSG_CIVPICK = 'K',   // client->host  i32 civ index (-1 = random)
    MSG_BYE     = 'Y',   // either        clean leave (u32 proto rides along)
    MSG_PING    = 'G',   // either        keepalive while the sim is stalled
};

// ---- connection state ----
static int  lsock = -1;          // host: listening socket
static int  sock  = -1;          // client: the connection to the host
static int  usock = -1;          // UDP: host responder / client discover
static bool isHost = false;
static bool matchActive = false;
static bool connLost = false;        // client: host gone. host: transport gone (web relay died)
static bool desynced = false;
static int  desyncTick = -1;
static bool sawStart = false;        // client: host pressed Begin
static bool cfgDirty = false;        // client: a fresh CONFIG arrived
static bool protoMismatch = false;   // BYE carried a different NET_PROTO_VERSION
static std::string peerName;         // client: the host's name
static std::string relayErr;         // web: last relay-side reason a connect failed
static NetMatchConfig cfg;

static std::vector<unsigned char> rxBuf, txBuf;   // client-role stream buffers
static long long lastRecvMs = 0;
static long long lastPingMs = 0;
static long long lastSendMs = 0;

// ---- host: one entry per challenger connection ----
struct HostPeer {
    bool used    = false;
    bool seated  = false;    // completed HELLO
    bool dropped = false;    // left mid-match; seat stays, contributes nothing
    int  fd      = -1;       // native TCP socket (web uses the slot index)
    int  seat    = -1;       // 1..MAX_PLAYERS-1 once seated
    int  joinSeq = 0;        // arrival order — seats stay in join order
    int  civPick = -1;
    std::string name;
    std::vector<unsigned char> rx, tx;
    long long lastRecv = 0, lastSend = 0;
    std::map<int, std::vector<Command>> inbox;        // execTick -> their commands
    std::map<int, unsigned long long>   hashes;       // tick -> their reported hash
};
static HostPeer hpeers[MAX_NET_CLIENTS];
static int joinSeqCounter = 0;

// ---- lockstep state ----
static int localSlot = 0;                              // our seat (host 0)
static std::map<int, std::vector<Command>> hostOwn;    // host: own scheduled commands
static std::map<int, std::vector<Command>> clientInbox;// client: merged TICKSETs
static std::vector<Command> localPending;              // issued since the last tick
static bool waitingForPeer = false;
static std::string waitingOn;                          // who the stall is on (UI)
static std::map<int, unsigned long long> localHashes;
static unsigned pausedMask = 0;                        // bit per seat (host aggregates)
static char rosterNames[MAX_PLAYERS][24] = {{0}};      // client: seat -> name

static void netMatchBegin(int slot);   // defined with the scheduler below
#ifdef __EMSCRIPTEN__
static void wsFlushTx();               // browser WebSocket transport, defined below
#endif

static long long nowMs() {
    struct timeval tv; gettimeofday(&tv, nullptr);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void setNonBlocking(int fd) {
#ifdef _WIN32
    u_long one = 1;
    ioctlsocket(fd, FIONBIO, &one);
#else
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
#endif
}

static void closeFd(int& fd) { if (fd >= 0) { closeRawSock(fd); fd = -1; } }

// REALM_NET_DEBUG=1: log why the link died (stderr; harness only).
static void netDbg(const char* where, int err) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("REALM_NET_DEBUG"); on = (e && *e && *e != '0') ? 1 : 0; }
    if (on) fprintf(stderr, "[net] connLost: %s (err=%d) tick=%d\n", where, err, g.tick);
}
#define CONN_LOST(where) do { if (!connLost) netDbg(where, netErrno()); connLost = true; } while (0)

static std::string localUserName() {
    const char* u = getenv("USER");
    if (!u || !*u) u = "player";
    return std::string(u).substr(0, 23);
}

int netSeatedCount() {
    int n = 0;
    for (auto& p : hpeers) if (p.used && p.seated && !p.dropped) n++;
    return n;
}
int netMySeat() { return localSlot; }

// Seat -> display name, both roles (host from peers, client from the roster).
std::string netSeatName(int seat) {
    if (seat == localSlot) return localUserName();
    if (isHost) {
        if (seat == 0) return localUserName();
        for (auto& p : hpeers) if (p.used && p.seated && p.seat == seat && !p.name.empty()) return p.name;
    } else {
        if (seat >= 0 && seat < MAX_PLAYERS && rosterNames[seat][0]) return rosterNames[seat];
    }
    char b[16]; snprintf(b, sizeof b, "Player %d", seat + 1);
    return b;
}

bool netActive() { return matchActive; }
bool netConnectionLost() { return connLost; }
bool netVersionMismatch() { return protoMismatch; }
int  netProtoVersion() { return (int)NET_PROTO_VERSION; }
bool netDesynced() { return desynced; }
int  netDesyncTick() { return desyncTick; }
bool netPeerPaused() { return (pausedMask & ~(1u << localSlot)) != 0; }
bool netWaitingForPeer() { return waitingForPeer; }

// Whose pause / whose stall — for the in-match banners.
std::string netPauseName() {
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (s != localSlot && (pausedMask >> s) & 1) return netSeatName(s);
    return "Opponent";
}
std::string netWaitingName() {
    if (!isHost) return peerName.empty() ? std::string("the host") : peerName;
    return waitingOn.empty() ? std::string("challengers") : waitingOn;
}

// Client: the host's name. Host: a short summary of the seated challengers.
std::string netPeerName() {
    if (!isHost) return peerName.empty() ? std::string("Opponent") : peerName;
    std::string out;
    int extra = 0;
    for (auto& p : hpeers) {
        if (!p.used || !p.seated || p.dropped) continue;
        if (out.empty()) out = p.name;
        else if (out.size() + p.name.size() < 30) out += ", " + p.name;
        else extra++;
    }
    if (extra) out += " +" + std::to_string(extra);
    return out.empty() ? std::string("Opponent") : out;
}

// ---- framing ----
// Client-role send: one stream to the host.
static void sendFrame(unsigned char type, const void* payload, unsigned len) {
    if (sock < 0) return;
    if (netTrace()) fprintf(stderr, "[net>] %c len=%u tick=%d txq=%zu\n", type, len, g.tick, txBuf.size());
    unsigned char hdr[5];
    memcpy(hdr, &len, 4);
    hdr[4] = type;
    txBuf.insert(txBuf.end(), hdr, hdr + 5);
    if (len) {
        const unsigned char* p = (const unsigned char*)payload;
        txBuf.insert(txBuf.end(), p, p + len);
    }
    // Flush what the socket will take; the rest stays queued for netPump.
    // (errWouldBlock, not raw errno — winsock reports through WSAGetLastError.)
#ifdef __EMSCRIPTEN__
    wsFlushTx();                         // hand the queue to the WebSocket
#else
    while (!txBuf.empty()) {
        ssize_t n = nsend(sock, txBuf.data(), txBuf.size());
        if (n > 0) txBuf.erase(txBuf.begin(), txBuf.begin() + n);
        else if (n < 0 && errWouldBlock()) break;
        else { CONN_LOST("send"); break; }
    }
#endif
    lastSendMs = nowMs();
}

// Host-role send: queue a frame for ONE peer connection...
static void sendFrameTo(HostPeer& p, unsigned char type, const void* payload, unsigned len) {
    if (!p.used) return;
    if (netTrace()) fprintf(stderr, "[net>%d] %c len=%u tick=%d\n", (int)(&p - hpeers), type, len, g.tick);
    unsigned char hdr[5];
    memcpy(hdr, &len, 4);
    hdr[4] = type;
    p.tx.insert(p.tx.end(), hdr, hdr + 5);
    if (len) {
        const unsigned char* b = (const unsigned char*)payload;
        p.tx.insert(p.tx.end(), b, b + len);
    }
#ifndef __EMSCRIPTEN__
    while (!p.tx.empty()) {
        ssize_t n = nsend(p.fd, p.tx.data(), p.tx.size());
        if (n > 0) p.tx.erase(p.tx.begin(), p.tx.begin() + n);
        else break;   // EAGAIN or error: netPump's flush settles it
    }
#else
    wsFlushTx();      // web: per-peer queues ride one relay socket
#endif
    p.lastSend = nowMs();
}

// ...or for every seated peer (dropped seats get nothing — they're gone).
static void hostBroadcast(unsigned char type, const void* payload, unsigned len) {
    for (auto& p : hpeers) if (p.used && p.seated && !p.dropped) sendFrameTo(p, type, payload, len);
}

// ============================================================
// BROWSER TRANSPORT (Emscripten) — WebSocket to the pairing relay.
// A tab can't open TCP or listen(), so everyone connects OUT to a relay
// (web-relay/) that groups them by room code. The joiner side is a plain
// byte pipe to the host. The HOST side is one multiplexed socket: the relay
// prefixes each joiner->host message with the joiner's index, and the host
// prefixes each host->joiner message with the destination index (the relay
// strips it). The relay never parses game bytes — it only routes them.
// `sock` is set to a non-negative sentinel while the socket is open so the
// shared `sock >= 0` guards hold — no real fd syscall is made on this build.
// ============================================================
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/websocket.h>
static EMSCRIPTEN_WEBSOCKET_T ws = 0;
static bool wsIsOpen    = false;
static bool wsSendHello = false;     // client: greet with HELLO once the socket opens

static void wsFlushTx() {
    if (!wsIsOpen || ws <= 0) return;
    if (!isHost) {
        if (txBuf.empty()) return;
        if (emscripten_websocket_send_binary(ws, txBuf.data(), (uint32_t)txBuf.size())
            == EMSCRIPTEN_RESULT_SUCCESS)
            txBuf.clear();           // else leave queued; retry on the next pump
        return;
    }
    // Host: each peer's queue goes out as [destIdx][bytes] messages.
    for (int i = 0; i < MAX_NET_CLIENTS; i++) {
        HostPeer& p = hpeers[i];
        if (!p.used || p.tx.empty()) continue;
        std::vector<unsigned char> msg;
        msg.reserve(1 + p.tx.size());
        msg.push_back((unsigned char)i);
        msg.insert(msg.end(), p.tx.begin(), p.tx.end());
        if (emscripten_websocket_send_binary(ws, msg.data(), (uint32_t)msg.size())
            == EMSCRIPTEN_RESULT_SUCCESS)
            p.tx.clear();
    }
}

static void sendHelloFrame() {
    unsigned char pl[4 + 24] = {0};
    unsigned proto = NET_PROTO_VERSION;
    memcpy(pl, &proto, 4);
    snprintf((char*)pl + 4, 24, "%s", localUserName().c_str());
    sendFrame(MSG_HELLO, pl, sizeof pl);
}

static void webPeerGone(int idx);      // defined with the host lobby below

static EM_BOOL wsOnOpen(int, const EmscriptenWebSocketOpenEvent*, void*) {
    wsIsOpen = true;
    sock = 1;                        // sentinel: "connected" for the shared guards
    lastRecvMs = lastSendMs = nowMs();
    wsFlushTx();
    if (wsSendHello) sendHelloFrame();   // client greets; host waits for HELLOs
    return EM_TRUE;
}
static EM_BOOL wsOnMessage(int, const EmscriptenWebSocketMessageEvent* e, void*) {
    if (e->isText) {                 // relay control frame
        if (e->numBytes >= 3 && memcmp(e->data, "ERR", 3) == 0) {
            // Keep the reason so the lobby can say WHY (wrong code, room full,
            // host not in their lobby yet) instead of "they closed the lobby".
            const char* msg = (const char*)e->data + (e->numBytes > 4 ? 4 : 3);
            relayErr.assign(msg, e->data + e->numBytes - (const uint8_t*)msg);
            CONN_LOST("relay");
        } else if (isHost && e->numBytes >= 4 && memcmp(e->data, "BYE", 3) == 0) {
            webPeerGone(atoi((const char*)e->data + 4));   // a joiner's pipe closed
        }
        return EM_TRUE;
    }
    if (isHost) {
        // [senderIdx][bytes] — demux into that peer's stream buffer.
        if (e->numBytes < 1) return EM_TRUE;
        int idx = e->data[0];
        if (idx < 0 || idx >= MAX_NET_CLIENTS) return EM_TRUE;
        HostPeer& p = hpeers[idx];
        if (!p.used) { p = HostPeer(); p.used = true; p.fd = -1; p.lastRecv = nowMs(); }
        p.rx.insert(p.rx.end(), e->data + 1, e->data + e->numBytes);
        p.lastRecv = nowMs();
    } else {
        rxBuf.insert(rxBuf.end(), e->data, e->data + e->numBytes);
        lastRecvMs = nowMs();
    }
    return EM_TRUE;
}
static EM_BOOL wsOnError(int, const EmscriptenWebSocketErrorEvent*, void*) {
    if (!wsIsOpen && relayErr.empty()) relayErr = "couldn't reach the relay (is it running? URL correct?)";
    CONN_LOST("ws-error"); return EM_TRUE;
}
static EM_BOOL wsOnClose(int, const EmscriptenWebSocketCloseEvent*, void*) {
    if (!wsIsOpen && relayErr.empty()) relayErr = "couldn't reach the relay (is it running? URL correct?)";
    CONN_LOST("ws-close"); wsIsOpen = false; sock = -1; return EM_TRUE;
}

static bool wsConnect(const std::string& url, bool asClient) {
    if (!emscripten_websocket_is_supported()) return false;
    netClose();
    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = url.c_str();
    ws = emscripten_websocket_new(&attr);
    if (ws <= 0) { ws = 0; return false; }
    wsIsOpen    = false;
    wsSendHello = asClient;
    lastRecvMs  = nowMs();
    emscripten_websocket_set_onopen_callback(ws, nullptr, wsOnOpen);
    emscripten_websocket_set_onmessage_callback(ws, nullptr, wsOnMessage);
    emscripten_websocket_set_onerror_callback(ws, nullptr, wsOnError);
    emscripten_websocket_set_onclose_callback(ws, nullptr, wsOnClose);
    return true;
}

// The page can hand us a relay at runtime: shell.html sets Module.relayUrl
// from a ?relay=… query parameter or a relay.json served beside index.html.
// One hosted build can then re-point at a new relay without recompiling;
// the compile-time REALM_RELAY_URL is only the last resort (localhost dev).
std::string realmRelayDefault() {
    const char* s = emscripten_run_script_string(
        "(typeof Module!=='undefined'&&Module.relayUrl)?String(Module.relayUrl):''");
    return (s && *s) ? s : REALM_RELAY_URL;
}

static std::string relayUrlFor(const char* relay, const char* room, const char* role) {
    std::string u = (relay && *relay) ? relay : realmRelayDefault();
    u += (u.find('?') == std::string::npos) ? "?" : "&";
    u += "room="; u += room; u += "&role="; u += role;
    return u;
}

bool netWebHost(const char* room, const char* relay) {
    netPlatformInit();
    // isHost MUST be set AFTER wsConnect — wsConnect calls netClose(), which
    // clears isHost. The HELLO handler only seats a client when isHost is true.
    bool ok = wsConnect(relayUrlFor(relay, room, "host"), false);
    isHost = true;
    return ok;
}
bool netWebJoin(const char* room, const char* relay, std::string& err) {
    netPlatformInit();
    if (!wsConnect(relayUrlFor(relay, room, "join"), true)) {
        err = "This browser has no WebSocket support."; return false;
    }
    isHost = false;
    return true;
}
#endif  // __EMSCRIPTEN__

// One settings blurb, shared by discovery replies and the lobbies.
static std::string cfgBlurb(const NetMatchConfig& c) {
    static const char* diffs[] = {"Easy", "Normal", "Hard"};
    char b[64];
    snprintf(b, sizeof b, "%d AI%s - %s", c.numAIs, c.numAIs == 1 ? "" : "s",
             diffs[std::max(0, std::min(2, c.difficulty))]);
    return b;
}

// ============================================================
// HOST LOBBY
// ============================================================

// Reseat everyone contiguously in join order (humans must occupy the low
// seats — initGame fills AI seats after the humans). Lobby-time only.
static void hostReseat() {
    std::vector<HostPeer*> order;
    for (auto& p : hpeers) if (p.used && p.seated) order.push_back(&p);
    std::sort(order.begin(), order.end(),
              [](const HostPeer* a, const HostPeer* b) { return a->joinSeq < b->joinSeq; });
    int seat = 1;
    for (auto* p : order) p->seat = seat++;
}

// Everyone's name sheet + their own seat number (payload byte 0 differs per
// recipient, so this can't ride hostBroadcast).
static void hostSendRosters() {
    unsigned char pl[1 + MAX_PLAYERS * 24] = {0};
    snprintf((char*)pl + 1, 24, "%s", localUserName().c_str());
    for (auto& p : hpeers)
        if (p.used && p.seated && p.seat > 0 && p.seat < MAX_PLAYERS)
            snprintf((char*)pl + 1 + p.seat * 24, 24, "%s", p.name.c_str());
    for (auto& p : hpeers) {
        if (!p.used || !p.seated || p.dropped) continue;
        pl[0] = (unsigned char)p.seat;
        sendFrameTo(p, MSG_ROSTER, pl, sizeof pl);
    }
}

bool netHostOpen() {
    netPlatformInit();
    netClose();
    isHost = true;

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) return false;
    int yes = 1;
    nsetsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(NET_TCP_PORT);
    if (bind(lsock, (sockaddr*)&a, sizeof a) < 0 || listen(lsock, MAX_NET_CLIENTS + 1) < 0) {
        closeFd(lsock); isHost = false; return false;
    }
    setNonBlocking(lsock);

    // Discovery responder: answer LAN broadcast pings with our lobby card.
    usock = socket(AF_INET, SOCK_DGRAM, 0);
    if (usock >= 0) {
        nsetsockopt(usock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        sockaddr_in u{};
        u.sin_family = AF_INET; u.sin_addr.s_addr = INADDR_ANY; u.sin_port = htons(NET_UDP_PORT);
        if (bind(usock, (sockaddr*)&u, sizeof u) < 0) closeFd(usock);
        else setNonBlocking(usock);
    }
    return true;
}

// cfg is menu-owned; net.cpp overlays what only it knows: which seats are
// human (join order) and each challenger's civ pick. AI seats are clamped
// to whatever chairs are left.
void netHostSetInfo(const NetMatchConfig& c) {
    cfg = c;
    int humans = 1 + netSeatedCount();
    cfg.humanMask = (1 << humans) - 1;
    cfg.numAIs = std::max(0, std::min(cfg.numAIs, MAX_PLAYERS - humans));
    for (auto& p : hpeers)
        if (p.used && p.seated && !p.dropped && p.seat > 0 && p.seat < MAX_PLAYERS)
            cfg.civ[p.seat] = p.civPick;
    if (netSeatedCount() > 0) {
        unsigned char pl[8 + (6 + MAX_PLAYERS) * 4];
        memcpy(pl, &cfg.seed, 8);
        int f[6 + MAX_PLAYERS] = {cfg.numAIs, cfg.biome, cfg.layout,
                                  cfg.difficulty, cfg.speed, cfg.humanMask};
        for (int i = 0; i < MAX_PLAYERS; i++) f[6 + i] = cfg.civ[i];
        memcpy(pl + 8, f, sizeof f);
        hostBroadcast(MSG_CONFIG, pl, sizeof pl);
    }
}

NetMatchConfig netFinalConfig() { return cfg; }

bool netHostClientPresent() { return netSeatedCount() > 0 && !connLost; }
std::string netHostClientName() { return netPeerName(); }

// A peer connection ended (EOF, error, relay BYE, frame-level BYE).
// In the lobby the chair is simply freed; mid-match the seat goes inert —
// the match continues and their forces stand down.
static void hostPeerGone(HostPeer& p, const char* why) {
    if (!p.used) return;
    netDbg(why, 0);
    if (matchActive && p.seated && !p.dropped) {
        p.dropped = true;
        p.inbox.clear();               // any queued future commands die with them
        closeFd(p.fd);
        p.rx.clear(); p.tx.clear();
        unsigned char pl[1 + 24] = {0};
        pl[0] = (unsigned char)p.seat;
        snprintf((char*)pl + 1, 24, "%s", p.name.c_str());
        hostBroadcast(MSG_DROP, pl, sizeof pl);
        setStatus(p.name + " has left the battle — their forces stand down.");
        pausedMask &= ~(1u << p.seat);
        return;
    }
    bool wasSeated = p.seated;
    closeFd(p.fd);
    p = HostPeer();
    if (wasSeated && !matchActive) {   // re-pack seats, tell everyone
        hostReseat();
        hostSendRosters();
        netHostSetInfo(cfg);
    }
}

#ifdef __EMSCRIPTEN__
static void webPeerGone(int idx) {
    if (idx >= 0 && idx < MAX_NET_CLIENTS) hostPeerGone(hpeers[idx], "relay-bye");
}
#endif

// Accept + handshake + answer discovery pings. Returns true while healthy.
bool netHostPoll() {
#ifndef __EMSCRIPTEN__
    // Discovery ping?
    if (usock >= 0) {
        char buf[16]; sockaddr_in from{}; socklen_t fl = sizeof from;
        ssize_t n;
        while ((n = nrecvfrom(usock, buf, sizeof buf, (sockaddr*)&from, &fl)) > 0) {
            if (n >= 4 && memcmp(buf, "RLM?", 4) == 0) {
                unsigned char re[4 + 4 + 2 + 24 + 40] = {'R','L','M','!'};
                unsigned proto = NET_PROTO_VERSION;
                memcpy(re + 4, &proto, 4);
                unsigned short port = NET_TCP_PORT;
                memcpy(re + 8, &port, 2);
                snprintf((char*)re + 10, 24, "%s", localUserName().c_str());
                int seats = netSeatedCount();
                snprintf((char*)re + 34, 40, "%s - %d/%d", cfgBlurb(cfg).c_str(),
                         seats, MAX_NET_CLIENTS);
                nsendto(usock, re, sizeof re, (sockaddr*)&from, fl);
            }
            fl = sizeof from;
        }
    }
#endif  // !__EMSCRIPTEN__
    netPump();   // accepts, HELLOs, reaping all live in the pump now
    return !connLost;
}

bool netHostStart() {
    if (netSeatedCount() < 1 || connLost) return false;
    netHostSetInfo(cfg);              // final settings, then the gun
    hostSendRosters();                // final seats — clients learn theirs here
    netMatchBegin(0);                 // seat + clean slate BEFORE the gun fires
    hostBroadcast(MSG_START, nullptr, 0);
    return !connLost;
}

// ============================================================
// CLIENT JOIN
// ============================================================
bool netJoinConnect(const char* addr, int port, std::string& err) {
    netPlatformInit();
    netClose();
    isHost = false;

    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    char portStr[8]; snprintf(portStr, sizeof portStr, "%d", port);
    if (getaddrinfo(addr, portStr, &hints, &res) != 0 || !res) {
        err = "Can't resolve that address."; return false;
    }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { freeaddrinfo(res); err = "socket() failed"; return false; }
    setNonBlocking(sock);
    int rc = connect(sock, res->ai_addr, (socklen_t)res->ai_addrlen);
    freeaddrinfo(res);
    if (rc < 0 && !errInProgress()) { closeFd(sock); err = "Connection refused."; return false; }
    // Wait up to 4s for the connect to land.
    fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf);
    timeval tv{4, 0};
    if (select(sock + 1, nullptr, &wf, nullptr, &tv) <= 0) {
        closeFd(sock); err = "No answer (is the host lobby open? firewall?)"; return false;
    }
    int soerr = 0; socklen_t sl = sizeof soerr;
    ngetsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl);
    if (soerr != 0) { closeFd(sock); err = "Connection refused (no lobby at that address)."; return false; }
    int yes = 1;
    nsetsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);

    unsigned char pl[4 + 24] = {0};
    unsigned proto = NET_PROTO_VERSION;
    memcpy(pl, &proto, 4);
    snprintf((char*)pl + 4, 24, "%s", localUserName().c_str());
    sendFrame(MSG_HELLO, pl, sizeof pl);
    lastRecvMs = nowMs();
    return !connLost;
}

int netClientPoll(NetMatchConfig& out) {
    netPump();
    if (connLost) return -1;
    if (sawStart) { out = cfg; sawStart = false; return 2; }
    if (cfgDirty) { out = cfg; cfgDirty = false; return 1; }
    return 0;
}

// ============================================================
// LAN DISCOVERY (client side)
// ============================================================
static std::vector<NetLobbyInfo> found;

void netDiscoverStart() {
    netPlatformInit();
    netDiscoverStop();
    usock = socket(AF_INET, SOCK_DGRAM, 0);
    if (usock < 0) return;
    int yes = 1;
    nsetsockopt(usock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof yes);
    setNonBlocking(usock);
    lastPingMs = 0;
    found.clear();
}

void netDiscoverPoll(std::vector<NetLobbyInfo>& out) {
    if (usock < 0) { out = found; return; }
    long long t = nowMs();
    if (t - lastPingMs > 1000) {
        lastPingMs = t;
        sockaddr_in b{};
        b.sin_family = AF_INET; b.sin_port = htons(NET_UDP_PORT);
        b.sin_addr.s_addr = INADDR_BROADCAST;
        nsendto(usock, "RLM?", 4, (sockaddr*)&b, sizeof b);
        // Loopback ping too, so a host on THIS machine shows up (testing).
        b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        nsendto(usock, "RLM?", 4, (sockaddr*)&b, sizeof b);
    }
    unsigned char buf[128]; sockaddr_in from{}; socklen_t fl = sizeof from;
    ssize_t n;
    while ((n = nrecvfrom(usock, buf, sizeof buf, (sockaddr*)&from, &fl)) > 0) {
        if (n >= (ssize_t)(4 + 4 + 2 + 24 + 40) && memcmp(buf, "RLM!", 4) == 0) {
            unsigned proto; memcpy(&proto, buf + 4, 4);
            if (proto == NET_PROTO_VERSION) {
                NetLobbyInfo li;
                char ip[32];
                inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
                li.addr = ip;
                unsigned short port; memcpy(&port, buf + 8, 2);
                li.port = port;
                li.host.assign((char*)buf + 10);
                li.map.assign((char*)buf + 34);
                bool dup = false;
                for (auto& e : found) if (e.addr == li.addr) { e = li; dup = true; }
                if (!dup) found.push_back(li);
            }
        }
        fl = sizeof from;
    }
    out = found;
}

void netDiscoverStop() {
    if (!isHost) closeFd(usock);
    found.clear();
}

std::vector<std::string> netLocalAddresses() {
    std::vector<std::string> out;
#if defined(__EMSCRIPTEN__)
    // Browser build never shows the host lobby; compile-only stub.
    return out;
#elif defined(_WIN32)
    // No getifaddrs on Windows; the lobby shows a hint instead of addresses.
    out.push_back("(run ipconfig for your address)");
    return out;
#else
    ifaddrs* ifs = nullptr;
    if (getifaddrs(&ifs) != 0) return out;
    for (ifaddrs* i = ifs; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        char ip[32];
        inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, ip, sizeof ip);
        if (strcmp(ip, "127.0.0.1") == 0) continue;
        out.push_back(ip);
    }
    freeifaddrs(ifs);
    return out;
#endif
}

// ============================================================
// MESSAGE PUMP
// ============================================================
// Decode a {execTick, nCmds, commands} payload (BUNDLE and TICKSET share the
// shape). false = malformed (caller decides who to disconnect).
static bool decodeBundle(const unsigned char* p, unsigned len,
                         int& execTick, std::vector<Command>& cmds) {
    if (len < 8) return false;
    int nCmds;
    memcpy(&execTick, p, 4); memcpy(&nCmds, p + 4, 4);
    if (nCmds < 0 || nCmds > 4096) return false;
    // A sane peer is never more than a pause-skew ahead; a corrupt
    // execTick would silently bloat the inbox map forever.
    if (execTick < 0 || execTick > g.tick + 1000) return false;
    // The frame header is 5 bytes, so the payload lands odd-aligned:
    // copy the command ints into an aligned buffer before decoding
    // (casting p+8 to int* is UB — UBSan-caught).
    int avail = (int)((len - 8) / 4);
    std::vector<int> fbuf(std::max(0, avail));
    if (avail > 0) memcpy(fbuf.data(), p + 8, (size_t)avail * 4);
    const int* f = fbuf.data();
    for (int i = 0; i < nCmds; i++) {
        Command c;
        int used = decodeCommand(f, avail, c);
        if (used <= 0) return false;
        f += used; avail -= used;
        cmds.push_back(std::move(c));
    }
    return true;
}

static void declareDesync(int tick) {
    if (desynced) return;
    desynced = true; desyncTick = tick;
    int t = tick;
    hostBroadcast(MSG_DESYNC, &t, 4);
}

// ---- frames arriving AT THE HOST from one peer ----
static void handleFrameHost(HostPeer& p, unsigned char type, const unsigned char* pl, unsigned len) {
    switch (type) {
    case MSG_HELLO: {
        if (len < 4 + 24) break;
        unsigned proto; memcpy(&proto, pl, 4);
        if (proto != NET_PROTO_VERSION) {
            // Tell the joiner WHY: our version rides in the BYE, so their
            // lobby can say "update Realm" instead of a shrug.
            unsigned pv = NET_PROTO_VERSION;
            sendFrameTo(p, MSG_BYE, &pv, 4);
            hostPeerGone(p, "proto");
            return;
        }
        if (matchActive) {           // a straggler found a room mid-match
            unsigned pv = NET_PROTO_VERSION;
            sendFrameTo(p, MSG_BYE, &pv, 4);
            hostPeerGone(p, "late-hello");
            return;
        }
        if (p.seated) break;         // duplicate HELLO: ignore
        p.name.assign((const char*)pl + 4, strnlen((const char*)pl + 4, 23));
        p.seated = true;
        p.joinSeq = ++joinSeqCounter;
        hostReseat();
        unsigned char re[4 + 24] = {0};
        memcpy(re, &proto, 4);
        snprintf((char*)re + 4, 24, "%s", localUserName().c_str());
        sendFrameTo(p, MSG_WELCOME, re, sizeof re);
        hostSendRosters();
        netHostSetInfo(cfg);         // current settings straight away
        break;
    }
    case MSG_CIVPICK:
        if (len >= 4) {
            memcpy(&p.civPick, pl, 4);
            if (p.civPick < -1 || p.civPick >= NUM_CIVS) p.civPick = -1;
            netHostSetInfo(cfg);     // echo the seat assignment to every lobby
        }
        break;
    case MSG_BUNDLE: {
        int execTick;
        std::vector<Command> cmds;
        if (!decodeBundle(pl, len, execTick, cmds)) { hostPeerGone(p, "bundle"); return; }
        p.inbox[execTick] = std::move(cmds);
        break;
    }
    case MSG_HASH: {
        if (len < 12) break;
        int tick; unsigned long long h;
        memcpy(&tick, pl, 4); memcpy(&h, pl + 4, 8);
        p.hashes[tick] = h;
        auto it = localHashes.find(tick);
        if (it != localHashes.end() && it->second != h) declareDesync(tick);
        while (p.hashes.size() > 8) p.hashes.erase(p.hashes.begin());
        break;
    }
    case MSG_PAUSE:
        if (len >= 1 && p.seat > 0) {
            if (pl[0]) pausedMask |= (1u << p.seat); else pausedMask &= ~(1u << p.seat);
            unsigned char m = (unsigned char)pausedMask;
            hostBroadcast(MSG_PAUSE, &m, 1);
        }
        break;
    case MSG_CHAT: {
        if (len < 2) break;
        std::string text((const char*)pl + 1, std::min(len - 1, 160u));
        setStatus(p.name + ": " + text);
        // Relay to the other challengers, stamped with the speaker's seat.
        std::vector<unsigned char> out(1 + text.size());
        out[0] = (unsigned char)p.seat;
        memcpy(out.data() + 1, text.data(), text.size());
        for (auto& q : hpeers)
            if (q.used && q.seated && !q.dropped && &q != &p)
                sendFrameTo(q, MSG_CHAT, out.data(), (unsigned)out.size());
        break;
    }
    case MSG_BYE:
        hostPeerGone(p, "bye");
        break;
    case MSG_PING:
        break;
    }
}

// ---- frames arriving AT A CLIENT from the host ----
static void handleFrameClient(unsigned char type, const unsigned char* p, unsigned len) {
    switch (type) {
    case MSG_WELCOME:
        if (len >= 4 + 24) peerName.assign((const char*)p + 4, strnlen((const char*)p + 4, 23));
        break;
    case MSG_CONFIG:
        if (len >= 8 + (6 + MAX_PLAYERS) * 4) {
            memcpy(&cfg.seed, p, 8);
            int f[6 + MAX_PLAYERS]; memcpy(f, p + 8, sizeof f);
            cfg.numAIs = f[0]; cfg.biome = f[1]; cfg.layout = f[2];
            cfg.difficulty = f[3]; cfg.speed = f[4]; cfg.humanMask = f[5];
            for (int i = 0; i < MAX_PLAYERS; i++) cfg.civ[i] = f[6 + i];
            cfgDirty = true;
        }
        break;
    case MSG_ROSTER:
        if (len >= 1 + MAX_PLAYERS * 24) {
            localSlot = std::max(1, std::min(MAX_PLAYERS - 1, (int)p[0]));
            for (int s = 0; s < MAX_PLAYERS; s++) {
                memcpy(rosterNames[s], p + 1 + s * 24, 24);
                rosterNames[s][23] = 0;
            }
            cfgDirty = true;   // lobby repaints the seat sheet
        }
        break;
    case MSG_START:
        netMatchBegin(localSlot);     // file every following tickset cleanly
        sawStart = true;
        break;
    case MSG_TICKSET: {
        int execTick;
        std::vector<Command> cmds;
        if (!decodeBundle(p, len, execTick, cmds)) { CONN_LOST("tickset-decode"); return; }
        clientInbox[execTick] = std::move(cmds);
        break;
    }
    case MSG_DESYNC:
        if (len >= 4) { memcpy(&desyncTick, p, 4); desynced = true; }
        break;
    case MSG_DROP:
        if (len >= 1 + 24) {
            int seat = p[0];
            char nm[24]; memcpy(nm, p + 1, 24); nm[23] = 0;
            setStatus(std::string(nm[0] ? nm : "A player") +
                      " has left the battle — their forces stand down.");
            if (seat >= 0 && seat < MAX_PLAYERS) pausedMask &= ~(1u << seat);
        }
        break;
    case MSG_PAUSE:
        if (len >= 1) pausedMask = p[0];
        break;
    case MSG_CHAT: {
        if (len < 2) break;
        std::string text((const char*)p + 1, std::min(len - 1, 160u));
        setStatus(netSeatName(p[0]) + ": " + text);
        break;
    }
    case MSG_BYE:
        // A versioned BYE (4-byte proto payload) that doesn't match ours
        // means the handshake failed on versions, not that the host quit.
        if (len >= 4) {
            unsigned pv; memcpy(&pv, p, 4);
            if (pv != NET_PROTO_VERSION) protoMismatch = true;
        }
        CONN_LOST("bye");
        break;
    case MSG_PING:
        break;
    }
}

// Parse complete frames out of one stream buffer. Returns false on a
// malformed stream (insane length header).
static bool parseFrames(std::vector<unsigned char>& buf, HostPeer* asPeer) {
    size_t off = 0;
    bool ok = true;
    while (buf.size() - off >= 5) {
        unsigned len; memcpy(&len, buf.data() + off, 4);
        if (len > 1 << 20) { ok = false; break; }        // insane frame: bail
        if (buf.size() - off < 5 + len) break;
        if (netTrace()) fprintf(stderr, "[net<%s] %c len=%u tick=%d\n",
                                asPeer ? "p" : "", buf[off + 4], len, g.tick);
        if (asPeer) handleFrameHost(*asPeer, buf[off + 4], buf.data() + off + 5, len);
        else        handleFrameClient(buf[off + 4], buf.data() + off + 5, len);
        off += 5 + len;
        if (asPeer && !asPeer->used) break;   // handler closed this peer
    }
    if (off) buf.erase(buf.begin(), buf.begin() + off);
    return ok;
}

static void pumpHost() {
#ifndef __EMSCRIPTEN__
    // Accept newcomers into free chairs. Mid-match there are no chairs:
    // greet-and-close so the stray joiner gets a clean "lobby closed".
    if (lsock >= 0) {
        int c;
        while ((c = accept(lsock, nullptr, nullptr)) >= 0) {
            HostPeer* slot = nullptr;
            if (!matchActive)
                for (auto& p : hpeers) if (!p.used) { slot = &p; break; }
            if (!slot) { closeRawSock(c); continue; }
            *slot = HostPeer();
            slot->used = true;
            slot->fd = c;
            setNonBlocking(c);
            int yes = 1;
            nsetsockopt(c, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
            slot->lastRecv = nowMs();
        }
    }
    for (auto& p : hpeers) {
        if (!p.used || p.fd < 0) continue;
        // Flush anything still queued for send.
        while (!p.tx.empty()) {
            ssize_t n = nsend(p.fd, p.tx.data(), p.tx.size());
            if (n > 0) p.tx.erase(p.tx.begin(), p.tx.begin() + n);
            else if (n < 0 && errWouldBlock()) break;
            else { hostPeerGone(p, "pump-send"); break; }
        }
        if (!p.used) continue;
        // Drain the socket.
        unsigned char buf[4096];
        ssize_t n;
        while ((n = nrecv(p.fd, buf, sizeof buf)) > 0) {
            p.rx.insert(p.rx.end(), buf, buf + n);
            p.lastRecv = nowMs();
        }
        if (n == 0) { hostPeerGone(p, "recv-eof"); continue; }        // orderly shutdown
        else if (n < 0 && !errWouldBlock()) { hostPeerGone(p, "recv-err"); continue; }
        if (!parseFrames(p.rx, &p)) { hostPeerGone(p, "frame-insane"); continue; }
    }
#else
    wsFlushTx();
    for (auto& p : hpeers) {
        if (!p.used) continue;
        if (!parseFrames(p.rx, &p)) { hostPeerGone(p, "frame-insane"); continue; }
    }
#endif
    long long t = nowMs();
    for (auto& p : hpeers) {
        if (!p.used || p.dropped) continue;
        // A connection that never says HELLO must not squat a chair
        // (port scanners, joiners whose game crashed mid-connect).
        if (!p.seated && t - p.lastRecv > 5000) { hostPeerGone(p, "hello-timeout"); continue; }
        if (!p.seated) continue;
        // Keepalive + per-peer silence: bundles double as keepalives at
        // ~12.5/s, so ten quiet seconds means THAT player is gone.
        if (matchActive && t - p.lastSend > 2500) sendFrameTo(p, MSG_PING, nullptr, 0);
        if (matchActive && t - p.lastRecv > 10000) hostPeerGone(p, "silence");
    }
}

static void pumpClient() {
    if (sock < 0) return;
#ifdef __EMSCRIPTEN__
    // The WebSocket callbacks do the IO: wsOnMessage has already appended any
    // received bytes into rxBuf; all we do here is push out the send queue.
    wsFlushTx();
#else
    // Flush anything still queued for send.
    while (!txBuf.empty()) {
        ssize_t n = nsend(sock, txBuf.data(), txBuf.size());
        if (n > 0) txBuf.erase(txBuf.begin(), txBuf.begin() + n);
        else if (n < 0 && errWouldBlock()) break;
        else { CONN_LOST("pump-send"); break; }
    }
    // Drain the socket.
    unsigned char buf[4096];
    ssize_t n;
    while ((n = nrecv(sock, buf, sizeof buf)) > 0) {
        rxBuf.insert(rxBuf.end(), buf, buf + n);
        lastRecvMs = nowMs();
    }
    if (n == 0) CONN_LOST("recv-eof");                        // orderly shutdown
    else if (n < 0 && !errWouldBlock()) CONN_LOST("recv-err");
#endif
    if (!parseFrames(rxBuf, nullptr)) CONN_LOST("frame-insane");
    // While the sim is stalled (pause, help sheet, waiting out a desync
    // check) no bundles flow — without a heartbeat, a >10s pause would
    // read as a dead peer and kill the match on BOTH sides.
    if (matchActive && !connLost && nowMs() - lastSendMs > 2500)
        sendFrame(MSG_PING, nullptr, 0);
    // Mid-match silence timeout: ten quiet seconds means the host is gone.
    if (matchActive && !connLost && nowMs() - lastRecvMs > 10000) CONN_LOST("silence");
}

void netPump() {
    if (isHost) pumpHost();
    else pumpClient();
}

// ============================================================
// LOCKSTEP SCHEDULER
// ============================================================
// Reset MUST happen the instant the match is agreed — host: before START
// leaves; client: the moment START is parsed — because the first TICKSETs
// can share a TCP segment with START itself. Resetting any later (say,
// after initGame) wipes early ticksets: the client stalls at tick
// NET_CMD_DELAY and dies of silence.
static void netMatchBegin(int slot) {
    localSlot = slot;
    matchActive = true;
    waitingForPeer = false;
    waitingOn.clear();
    desynced = false; desyncTick = -1;
    pausedMask = 0;
    hostOwn.clear(); clientInbox.clear();
    localPending.clear();
    localHashes.clear();
    // The first NET_CMD_DELAY ticks have no scheduled commands by
    // construction — pre-seed empty bundles so everyone can roll.
    if (isHost) {
        for (auto& p : hpeers) if (p.used && p.seated) p.inbox.clear();
        for (int t = 1; t <= NET_CMD_DELAY; t++) {
            hostOwn[t];
            for (auto& p : hpeers) if (p.used && p.seated) p.inbox[t];
        }
    } else {
        for (int t = 1; t <= NET_CMD_DELAY; t++) clientInbox[t];
    }
    for (auto& p : hpeers) p.hashes.clear();
    lastRecvMs = nowMs();
}

void netQueueLocal(const Command& c) {
    localPending.push_back(c);
}

// May tick k = g.tick+1 proceed?
//  host:   yes once every live seat's bundle for k is in — then merge in
//          seat order, broadcast the TICKSET, and queue the merged stream.
//  client: yes once the host's TICKSET for k has arrived.
bool netTickReady() {
    netPump();
    if (connLost || desynced) return false;
    int k = g.tick + 1;
    if (isHost) {
        auto own = hostOwn.find(k);
        if (own == hostOwn.end()) return false;
        for (auto& p : hpeers) {
            if (!p.used || !p.seated || p.dropped) continue;
            if (!p.inbox.count(k)) { waitingForPeer = true; waitingOn = p.name; return false; }
        }
        waitingForPeer = false;
        // Canonical stream: host seat first, then seats ascending — fixed
        // order, so arrival order can't desync anyone.
        std::vector<Command> merged = std::move(own->second);
        hostOwn.erase(own);
        for (int s = 1; s < MAX_PLAYERS; s++) {
            for (auto& p : hpeers) {
                if (!p.used || !p.seated || p.seat != s) continue;
                auto it = p.inbox.find(k);
                if (it != p.inbox.end()) {
                    merged.insert(merged.end(), it->second.begin(), it->second.end());
                    p.inbox.erase(it);
                }
            }
        }
        // Ticks 1..D are pre-seeded empty on every client — don't resend them.
        if (k > NET_CMD_DELAY) {
            std::vector<int> f;
            for (auto& c : merged) encodeCommand(c, f);
            std::vector<unsigned char> pl(8 + f.size() * 4);
            int n = (int)merged.size();
            memcpy(pl.data(), &k, 4);
            memcpy(pl.data() + 4, &n, 4);
            if (!f.empty()) memcpy(pl.data() + 8, f.data(), f.size() * 4);
            hostBroadcast(MSG_TICKSET, pl.data(), (unsigned)pl.size());
        }
        for (auto& c : merged) g.pendingCmds.push_back(c);
        return true;
    } else {
        auto it = clientInbox.find(k);
        if (it == clientInbox.end()) { waitingForPeer = true; return false; }
        waitingForPeer = false;
        for (auto& c : it->second) g.pendingCmds.push_back(c);
        clientInbox.erase(it);
        return true;
    }
}

// After simulating tick k:
//  client: ship our bundle for k+D (empty ones included — they're the
//          keepalive AND the host's permission to advance).
//  host:   file our own commands for k+D (the TICKSET carries them out).
// Every 100th tick both sides bank the state hash; clients report theirs
// and the host arbitrates.
void netAfterTick() {
    if (!matchActive) return;
    int execTick = g.tick + NET_CMD_DELAY;
    if (isHost) {
        hostOwn[execTick] = std::move(localPending);
    } else {
        std::vector<int> f;
        for (auto& c : localPending) encodeCommand(c, f);
        std::vector<unsigned char> pl(8 + f.size() * 4);
        int n = (int)localPending.size();
        memcpy(pl.data(), &execTick, 4);
        memcpy(pl.data() + 4, &n, 4);
        if (!f.empty()) memcpy(pl.data() + 8, f.data(), f.size() * 4);
        sendFrame(MSG_BUNDLE, pl.data(), (unsigned)pl.size());
    }
    localPending.clear();

    if (g.tick % 100 == 0) {
        unsigned long long h = simStateHash();
        localHashes[g.tick] = h;
        if (isHost) {
            for (auto& p : hpeers) {
                if (!p.used || !p.seated || p.dropped) continue;
                auto it = p.hashes.find(g.tick);
                if (it != p.hashes.end() && it->second != h) declareDesync(g.tick);
            }
        } else {
            unsigned char hp[12];
            memcpy(hp, &g.tick, 4); memcpy(hp + 4, &h, 8);
            sendFrame(MSG_HASH, hp, sizeof hp);
        }
        // Keep the hash books small.
        while (localHashes.size() > 8) localHashes.erase(localHashes.begin());
    }
}

void netSendChat(const std::string& text) {
    if (text.empty()) return;
    size_t n = std::min<size_t>(text.size(), 160);
    std::vector<unsigned char> pl(1 + n);
    pl[0] = (unsigned char)localSlot;
    memcpy(pl.data() + 1, text.data(), n);
    if (isHost) hostBroadcast(MSG_CHAT, pl.data(), (unsigned)pl.size());
    else sendFrame(MSG_CHAT, pl.data(), (unsigned)pl.size());
}

void netSendCivPick(int civ) {
    sendFrame(MSG_CIVPICK, &civ, 4);
}

void netSendPause(bool paused) {
    if (isHost) {
        if (paused) pausedMask |= 1u; else pausedMask &= ~1u;
        unsigned char m = (unsigned char)pausedMask;
        hostBroadcast(MSG_PAUSE, &m, 1);
    } else {
        unsigned char b = paused ? 1 : 0;
        sendFrame(MSG_PAUSE, &b, 1);
    }
}

void netSendBye() {
    unsigned proto = NET_PROTO_VERSION;
    if (isHost) hostBroadcast(MSG_BYE, &proto, 4);
    else sendFrame(MSG_BYE, &proto, 4);
}

void netClose() {
#ifdef __EMSCRIPTEN__
    // Close the relay socket and clear the sentinel so the shared closeFd(sock)
    // below is a no-op (sock is a fake handle on web — never a real fd).
    if (ws > 0) { emscripten_websocket_close(ws, 1000, ""); emscripten_websocket_delete(ws); }
    ws = 0; wsIsOpen = false; sock = -1;
#endif
    closeFd(sock); closeFd(lsock); closeFd(usock);
    for (auto& p : hpeers) { closeFd(p.fd); p = HostPeer(); }
    joinSeqCounter = 0;
    rxBuf.clear(); txBuf.clear();
    isHost = matchActive = connLost = desynced = false;
    sawStart = cfgDirty = waitingForPeer = false;
    protoMismatch = false;
    pausedMask = 0;
    localSlot = 0;
    peerName.clear();
    waitingOn.clear();
    relayErr.clear();
    memset(rosterNames, 0, sizeof rosterNames);
    hostOwn.clear(); clientInbox.clear();
    localPending.clear(); localHashes.clear();
    found.clear();
}

// Web: the relay's reason a connect failed (empty if none / native build).
std::string netRelayError() { return relayErr; }
