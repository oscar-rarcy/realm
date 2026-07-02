#include "realm.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdio>

// ============================================================
// LOCKSTEP NETWORKING (docs/networking-plan.md)
//
// Nothing but Commands ever crosses the wire: both machines run the whole
// deterministic sim (AIs included) from a shared seed, and a command issued
// at tick T executes on BOTH at T+NET_CMD_DELAY. TCP keeps the stream
// ordered and reliable; at an 80ms tick its latency is irrelevant on LAN
// and fine across a Tailscale/port-forward link. Same-binary-same-arch is
// a documented constraint (floats in the sim, host-endian ints on the
// wire) — matching NET_PROTO_VERSION is required at the handshake.
// ============================================================

static const unsigned NET_PROTO_VERSION = 2;   // bump on ANY wire or sim-format change (v2: eras/civs)

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
    MSG_CONFIG  = 'C',   // host->client  NetMatchConfig (u64 + 7 x i32)
    MSG_START   = 'S',   // host->client  begin the match
    MSG_BUNDLE  = 'B',   // both ways     i32 execTick, i32 nCmds, commands (codec ints)
    MSG_HASH    = 'A',   // both ways     i32 tick, u64 simStateHash
    MSG_PAUSE   = 'P',   // both ways     u8 paused
    MSG_CHAT    = 'T',   // both ways     utf-8 text (control channel, never sim)
    MSG_CIVPICK = 'K',   // client->host  i32 civ index (-1 = random)
    MSG_BYE     = 'Y',   // either        clean leave
};

// ---- connection state ----
static int  lsock = -1;          // host: listening socket
static int  sock  = -1;          // the peer connection
static int  usock = -1;          // UDP: host responder / client discover
static bool isHost = false;
static bool matchActive = false;
static bool connLost = false;
static bool desynced = false;
static int  desyncTick = -1;
static bool peerPaused = false;
static bool clientSeated = false;    // host: a client completed HELLO
static bool sawStart = false;        // client: host pressed Begin
static bool cfgDirty = false;        // client: a fresh CONFIG arrived
static std::string peerName;
static NetMatchConfig cfg;
static int clientCivPick = -1;       // host: the challenger's declared civ

static std::vector<unsigned char> rxBuf, txBuf;
static long long lastRecvMs = 0;
static long long lastPingMs = 0;

// ---- lockstep state ----
static int localSlot = 0;
static std::map<int, std::vector<Command>> inbox[2];  // execTick -> commands, per seat
static std::vector<Command> localPending;             // issued since the last tick
static bool waitingForPeer = false;
static std::map<int, unsigned long long> localHashes, remoteHashes;

static void netMatchBegin(int slot);   // defined with the scheduler below

static long long nowMs() {
    struct timeval tv; gettimeofday(&tv, nullptr);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void setNonBlocking(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static void closeFd(int& fd) { if (fd >= 0) { close(fd); fd = -1; } }

// REALM_NET_DEBUG=1: log why the link died (stderr; harness only).
static void netDbg(const char* where, int err) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("REALM_NET_DEBUG"); on = (e && *e && *e != '0') ? 1 : 0; }
    if (on) fprintf(stderr, "[net] connLost: %s (errno=%d %s) tick=%d\n", where, err, strerror(err), g.tick);
}
#define CONN_LOST(where) do { if (!connLost) netDbg(where, errno); connLost = true; } while (0)

bool netActive() { return matchActive; }
bool netConnectionLost() { return connLost; }
bool netDesynced() { return desynced; }
int  netDesyncTick() { return desyncTick; }
bool netPeerPaused() { return peerPaused; }
bool netWaitingForPeer() { return waitingForPeer; }
std::string netPeerName() { return peerName.empty() ? std::string("Opponent") : peerName; }

// ---- framing ----
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
    while (!txBuf.empty()) {
        ssize_t n = send(sock, txBuf.data(), txBuf.size(), 0);
        if (n > 0) txBuf.erase(txBuf.begin(), txBuf.begin() + n);
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        else { connLost = true; break; }
    }
}

static std::string localUserName() {
    const char* u = getenv("USER");
    if (!u || !*u) u = "player";
    return std::string(u).substr(0, 23);
}

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
bool netHostOpen() {
    netClose();
    isHost = true;

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) return false;
    int yes = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(NET_TCP_PORT);
    if (bind(lsock, (sockaddr*)&a, sizeof a) < 0 || listen(lsock, 1) < 0) {
        closeFd(lsock); isHost = false; return false;
    }
    setNonBlocking(lsock);

    // Discovery responder: answer LAN broadcast pings with our lobby card.
    usock = socket(AF_INET, SOCK_DGRAM, 0);
    if (usock >= 0) {
        setsockopt(usock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        sockaddr_in u{};
        u.sin_family = AF_INET; u.sin_addr.s_addr = INADDR_ANY; u.sin_port = htons(NET_UDP_PORT);
        if (bind(usock, (sockaddr*)&u, sizeof u) < 0) closeFd(usock);
        else setNonBlocking(usock);
    }
    return true;
}

void netHostSetInfo(const NetMatchConfig& c) {
    cfg = c;
    cfg.civ[1] = clientCivPick;      // the challenger's seat is theirs to pick
    if (clientSeated) {
        unsigned char pl[8 + 10 * 4];
        memcpy(pl, &cfg.seed, 8);
        int f[10] = {cfg.numAIs, cfg.biome, cfg.layout, cfg.difficulty, cfg.speed, cfg.humanMask,
                     cfg.civ[0], cfg.civ[1], cfg.civ[2], cfg.civ[3]};
        memcpy(pl + 8, f, sizeof f);
        sendFrame(MSG_CONFIG, pl, sizeof pl);
    }
}

bool netHostClientPresent() { return clientSeated && !connLost; }
std::string netHostClientName() { return peerName; }

// Accept + handshake + answer discovery pings. Returns true while healthy.
bool netHostPoll() {
    // Discovery ping?
    if (usock >= 0) {
        char buf[16]; sockaddr_in from{}; socklen_t fl = sizeof from;
        ssize_t n;
        while ((n = recvfrom(usock, buf, sizeof buf, 0, (sockaddr*)&from, &fl)) > 0) {
            if (n >= 4 && memcmp(buf, "RLM?", 4) == 0) {
                unsigned char re[4 + 4 + 2 + 24 + 40] = {'R','L','M','!'};
                unsigned proto = NET_PROTO_VERSION;
                memcpy(re + 4, &proto, 4);
                unsigned short port = NET_TCP_PORT;
                memcpy(re + 8, &port, 2);
                snprintf((char*)re + 10, 24, "%s", localUserName().c_str());
                snprintf((char*)re + 34, 40, "%s%s", cfgBlurb(cfg).c_str(),
                         clientSeated ? " (full)" : "");
                sendto(usock, re, sizeof re, 0, (sockaddr*)&from, fl);
            }
            fl = sizeof from;
        }
    }
    // Seat a connecting client.
    if (sock < 0 && lsock >= 0) {
        int c = accept(lsock, nullptr, nullptr);
        if (c >= 0) {
            sock = c;
            setNonBlocking(sock);
            int yes = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
            lastRecvMs = nowMs();
        }
    }
    netPump();   // may complete the HELLO
    return !connLost;
}

bool netHostStart() {
    if (!clientSeated || connLost) return false;
    netHostSetInfo(cfg);              // final settings, then the gun
    netMatchBegin(0);                 // seat + clean slate BEFORE the gun fires
    sendFrame(MSG_START, nullptr, 0);
    return !connLost;
}

// ============================================================
// CLIENT JOIN
// ============================================================
bool netJoinConnect(const char* addr, int port, std::string& err) {
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
    int rc = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc < 0 && errno != EINPROGRESS) { closeFd(sock); err = "Connection refused."; return false; }
    // Wait up to 4s for the connect to land.
    fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf);
    timeval tv{4, 0};
    if (select(sock + 1, nullptr, &wf, nullptr, &tv) <= 0) {
        closeFd(sock); err = "No answer (is the host lobby open? firewall?)"; return false;
    }
    int soerr = 0; socklen_t sl = sizeof soerr;
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl);
    if (soerr != 0) { closeFd(sock); err = "Connection refused (no lobby at that address)."; return false; }
    int yes = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);

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
    netDiscoverStop();
    usock = socket(AF_INET, SOCK_DGRAM, 0);
    if (usock < 0) return;
    int yes = 1;
    setsockopt(usock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof yes);
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
        sendto(usock, "RLM?", 4, 0, (sockaddr*)&b, sizeof b);
        // Loopback ping too, so a host on THIS machine shows up (testing).
        b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sendto(usock, "RLM?", 4, 0, (sockaddr*)&b, sizeof b);
    }
    unsigned char buf[128]; sockaddr_in from{}; socklen_t fl = sizeof from;
    ssize_t n;
    while ((n = recvfrom(usock, buf, sizeof buf, 0, (sockaddr*)&from, &fl)) > 0) {
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
}

// ============================================================
// MESSAGE PUMP
// ============================================================
static void handleFrame(unsigned char type, const unsigned char* p, unsigned len) {
    switch (type) {
    case MSG_HELLO:
        if (isHost && len >= 4 + 24) {
            unsigned proto; memcpy(&proto, p, 4);
            if (proto != NET_PROTO_VERSION) { sendFrame(MSG_BYE, nullptr, 0); CONN_LOST("proto"); return; }
            peerName.assign((const char*)p + 4);
            clientSeated = true;
            unsigned char re[4 + 24] = {0};
            memcpy(re, &proto, 4);
            snprintf((char*)re + 4, 24, "%s", localUserName().c_str());
            sendFrame(MSG_WELCOME, re, sizeof re);
            netHostSetInfo(cfg);   // current settings straight away
        }
        break;
    case MSG_WELCOME:
        if (!isHost && len >= 4 + 24) peerName.assign((const char*)p + 4);
        break;
    case MSG_CONFIG:
        if (!isHost && len >= 8 + 10 * 4) {
            memcpy(&cfg.seed, p, 8);
            int f[10]; memcpy(f, p + 8, sizeof f);
            cfg.numAIs = f[0]; cfg.biome = f[1]; cfg.layout = f[2];
            cfg.difficulty = f[3]; cfg.speed = f[4]; cfg.humanMask = f[5];
            for (int i = 0; i < MAX_PLAYERS; i++) cfg.civ[i] = f[6 + i];
            cfgDirty = true;
        }
        break;
    case MSG_CIVPICK:
        if (isHost && len >= 4) {
            memcpy(&clientCivPick, p, 4);
            if (clientCivPick < -1 || clientCivPick >= NUM_CIVS) clientCivPick = -1;
            netHostSetInfo(cfg);     // echo the seat assignment to both lobbies
        }
        break;
    case MSG_START:
        if (!isHost) {
            netMatchBegin(1);         // file every following bundle under the right seat
            sawStart = true;
        }
        break;
    case MSG_BUNDLE: {
        if (len < 8) break;
        int execTick, nCmds;
        memcpy(&execTick, p, 4); memcpy(&nCmds, p + 4, 4);
        if (nCmds < 0 || nCmds > 4096) { CONN_LOST("bundle-count"); break; }
        const int* f = (const int*)(p + 8);
        int avail = (int)((len - 8) / 4);
        std::vector<Command> cmds;
        for (int i = 0; i < nCmds; i++) {
            Command c;
            int used = decodeCommand(f, avail, c);
            if (used <= 0) { CONN_LOST("bundle-decode"); return; }
            f += used; avail -= used;
            cmds.push_back(std::move(c));
        }
        inbox[1 - localSlot][execTick] = std::move(cmds);
        break;
    }
    case MSG_HASH: {
        if (len < 12) break;
        int tick; unsigned long long h;
        memcpy(&tick, p, 4); memcpy(&h, p + 4, 8);
        remoteHashes[tick] = h;
        auto it = localHashes.find(tick);
        if (it != localHashes.end() && it->second != h) { desynced = true; desyncTick = tick; }
        break;
    }
    case MSG_PAUSE:
        peerPaused = (len >= 1 && p[0] != 0);
        break;
    case MSG_CHAT: {
        std::string text((const char*)p, std::min(len, 160u));
        setStatus(netPeerName() + ": " + text);
        break;
    }
    case MSG_BYE:
        CONN_LOST("bye");
        break;
    }
}

void netPump() {
    if (sock < 0) return;
    // Flush anything still queued for send.
    while (!txBuf.empty()) {
        ssize_t n = send(sock, txBuf.data(), txBuf.size(), 0);
        if (n > 0) txBuf.erase(txBuf.begin(), txBuf.begin() + n);
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        else { CONN_LOST("pump-send"); break; }
    }
    // Drain the socket.
    unsigned char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof buf, 0)) > 0) {
        rxBuf.insert(rxBuf.end(), buf, buf + n);
        lastRecvMs = nowMs();
    }
    if (n == 0) CONN_LOST("recv-eof");                        // orderly shutdown
    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) CONN_LOST("recv-err");
    // Parse complete frames.
    size_t off = 0;
    while (rxBuf.size() - off >= 5) {
        unsigned len; memcpy(&len, rxBuf.data() + off, 4);
        if (len > 1 << 20) { CONN_LOST("frame-insane"); break; }        // insane frame: bail
        if (rxBuf.size() - off < 5 + len) break;
        if (netTrace()) fprintf(stderr, "[net<] %c len=%u tick=%d\n", rxBuf[off + 4], len, g.tick);
        handleFrame(rxBuf[off + 4], rxBuf.data() + off + 5, len);
        off += 5 + len;
    }
    if (off) rxBuf.erase(rxBuf.begin(), rxBuf.begin() + off);
    // Mid-match silence timeout: bundles double as keepalives at ~12.5/s,
    // so ten quiet seconds means the peer is gone (not just slow).
    if (matchActive && !connLost && nowMs() - lastRecvMs > 10000) CONN_LOST("silence");
}

// ============================================================
// LOCKSTEP SCHEDULER
// ============================================================
// Reset MUST happen the instant the match is agreed — host: before START
// leaves; client: the moment START is parsed — because the peer's first
// bundles can share a TCP segment with START itself. Resetting any later
// (say, after initGame) files or wipes early bundles under the wrong seat:
// the client stalls at tick NET_CMD_DELAY and both sides die of silence.
static void netMatchBegin(int slot) {
    localSlot = slot;
    matchActive = true;
    waitingForPeer = false;
    desynced = false; desyncTick = -1;
    peerPaused = false;
    inbox[0].clear(); inbox[1].clear();
    localPending.clear();
    localHashes.clear(); remoteHashes.clear();
    // The first NET_CMD_DELAY ticks have no scheduled commands by
    // construction — pre-seed empty bundles so both sides can roll.
    for (int t = 1; t <= NET_CMD_DELAY; t++) { inbox[0][t]; inbox[1][t]; }
    lastRecvMs = nowMs();
}

void netQueueLocal(const Command& c) {
    localPending.push_back(c);
}

// May tick k = g.tick+1 proceed? Pump the wire, and if both seats' bundles
// for k are here, queue them (host seat first — fixed order, so arrival
// order can't desync) and say yes.
bool netTickReady() {
    netPump();
    if (connLost || desynced) return false;
    int k = g.tick + 1;
    auto a = inbox[0].find(k), b = inbox[1].find(k);
    if (a == inbox[0].end() || b == inbox[1].end()) { waitingForPeer = true; return false; }
    waitingForPeer = false;
    for (auto& c : a->second) g.pendingCmds.push_back(c);
    for (auto& c : b->second) g.pendingCmds.push_back(c);
    inbox[0].erase(a); inbox[1].erase(b);
    return true;
}

// After simulating tick k: ship our bundle for k+D (empty ones included —
// they're the keepalive AND the peer's permission to advance), plus the
// desync-alarm hash every 100th tick.
void netAfterTick() {
    if (!matchActive) return;
    int execTick = g.tick + NET_CMD_DELAY;
    std::vector<int> f;
    for (auto& c : localPending) encodeCommand(c, f);
    std::vector<unsigned char> pl(8 + f.size() * 4);
    int n = (int)localPending.size();
    memcpy(pl.data(), &execTick, 4);
    memcpy(pl.data() + 4, &n, 4);
    if (!f.empty()) memcpy(pl.data() + 8, f.data(), f.size() * 4);
    sendFrame(MSG_BUNDLE, pl.data(), (unsigned)pl.size());
    inbox[localSlot][execTick] = std::move(localPending);
    localPending.clear();

    if (g.tick % 100 == 0) {
        unsigned long long h = simStateHash();
        localHashes[g.tick] = h;
        auto it = remoteHashes.find(g.tick);
        if (it != remoteHashes.end() && it->second != h) { desynced = true; desyncTick = g.tick; }
        unsigned char hp[12];
        memcpy(hp, &g.tick, 4); memcpy(hp + 4, &h, 8);
        sendFrame(MSG_HASH, hp, sizeof hp);
        // Keep the hash books small.
        for (auto m : {&localHashes, &remoteHashes})
            while (m->size() > 8) m->erase(m->begin());
    }
}

void netSendChat(const std::string& text) {
    if (text.empty()) return;
    sendFrame(MSG_CHAT, text.data(), (unsigned)std::min<size_t>(text.size(), 160));
}

void netSendCivPick(int civ) {
    sendFrame(MSG_CIVPICK, &civ, 4);
}

void netSendPause(bool paused) {
    unsigned char b = paused ? 1 : 0;
    sendFrame(MSG_PAUSE, &b, 1);
}

void netSendBye() {
    sendFrame(MSG_BYE, nullptr, 0);
}

void netClose() {
    closeFd(sock); closeFd(lsock); closeFd(usock);
    rxBuf.clear(); txBuf.clear();
    isHost = matchActive = connLost = desynced = false;
    clientSeated = sawStart = cfgDirty = peerPaused = waitingForPeer = false;
    peerName.clear();
    clientCivPick = -1;
    inbox[0].clear(); inbox[1].clear();
    localPending.clear(); localHashes.clear(); remoteHashes.clear();
    found.clear();
}
