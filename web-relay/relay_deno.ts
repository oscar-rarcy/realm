// relay_deno.ts — Realm web-multiplayer relay for Deno / Deno Deploy.
//
// Same room-code hub as relay.js, built on Deno's native WebSocket server:
// one host plus up to MAX_JOIN challengers per room (4-player games). The
// relay never parses the game protocol — it only routes bytes:
//   joiner -> host   forwarded with the joiner's slot index prepended
//   host -> joiner   host prepends the destination index; relay strips it
// Control text: "ERR <reason>" before a rejection; the host gets "BYE <idx>"
// when a joiner closes. The room dies with the host; a joiner leaving just
// frees its slot.
//
//   Local:        deno run --allow-net --allow-env relay_deno.ts
//                 -> ws://localhost:7523
//   Deno Deploy:  `deployctl deploy --entrypoint relay_deno.ts`
//                 -> wss://<your-project>.deno.dev
//
// Clients connect to  <url>/?room=CODE&role=host|join   (see web-relay/README).

const MAX_JOIN = 3; // matches MAX_NET_CLIENTS in the game

interface Room { host?: WebSocket; joins: (WebSocket | null)[]; }
const rooms = new Map<string, Room>();

function handle(req: Request): Response {
  if (req.headers.get("upgrade") !== "websocket") {
    return new Response(
      "Realm relay is up. Connect a WebSocket: /?room=CODE&role=host|join\n",
    );
  }

  const { searchParams } = new URL(req.url);
  const code = (searchParams.get("room") || "").trim();
  const role = searchParams.get("role");
  const { socket, response } = Deno.upgradeWebSocket(req);
  socket.binaryType = "arraybuffer";

  // Decide accept/reject synchronously so a fast joiner finds the host.
  // Occupancy is "slot non-null", NEVER readyState: Deno server sockets sit
  // in CONNECTING until after this handler returns, so a readyState test
  // would see every just-seated player as a free chair (all joins landed on
  // slot 0 — the 4-player test caught it). teardown() nulls slots on close.
  let reject: string | null = null;
  let room = rooms.get(code);
  let idx = -1;
  if (!code || (role !== "host" && role !== "join")) {
    reject = "ERR bad request";
  } else if (role === "host") {
    if (room?.host) {
      reject = "ERR room code already in use";
    } else {
      room = room ?? { joins: new Array(MAX_JOIN).fill(null) };
      room.host = socket;
      rooms.set(code, room);
    }
  } else {
    if (!room || !room.host) {
      reject = "ERR no such room (has your friend opened the lobby?)";
    } else {
      idx = room.joins.findIndex((j) => !j);
      if (idx < 0) reject = "ERR room is full";
      else room.joins[idx] = socket;
    }
  }

  if (reject) {
    // Rejected sockets get no teardown handler, so they can't free a room
    // that isn't theirs.
    socket.onopen = () => { socket.send(reject!); socket.close(); };
    return response;
  }

  socket.onopen = () =>
    console.log(`[${code}] ${role}${idx >= 0 ? idx : ""} connected`);

  socket.onmessage = (e) => {
    const r = rooms.get(code);
    if (!r || typeof e.data === "string") return;   // game traffic is binary-only
    const bytes = new Uint8Array(e.data as ArrayBuffer);
    if (r.host === socket) {
      if (bytes.length < 1) return;                 // [destIdx][bytes]
      const peer = bytes[0] < MAX_JOIN ? r.joins[bytes[0]] : null;
      if (peer && peer.readyState === WebSocket.OPEN)
        peer.send((e.data as ArrayBuffer).slice(1));
    } else {
      const i = r.joins.indexOf(socket);
      if (i < 0 || !r.host || r.host.readyState !== WebSocket.OPEN) return;
      const out = new Uint8Array(1 + bytes.length);
      out[0] = i;
      out.set(bytes, 1);
      r.host.send(out);
    }
  };

  const teardown = () => {
    const r = rooms.get(code);
    if (!r) return;
    if (r.host === socket) {
      r.host = undefined;
      for (const j of r.joins) { try { j?.close(); } catch { /* gone */ } }
      rooms.delete(code);
      console.log(`[${code}] closed`);
    } else {
      const i = r.joins.indexOf(socket);
      if (i < 0) return;
      r.joins[i] = null;
      if (r.host && r.host.readyState === WebSocket.OPEN) r.host.send(`BYE ${i}`);
      console.log(`[${code}] join${i} left`);
    }
  };
  socket.onclose = teardown;
  socket.onerror = teardown;

  return response;
}

const port = Number(Deno.env.get("PORT") ?? "7523");
Deno.serve({ port }, handle);
console.log(`Realm relay (Deno) listening on :${port}`);
