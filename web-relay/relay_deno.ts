// relay_deno.ts — Realm web-multiplayer relay for Deno / Deno Deploy.
//
// Same room-code byte-pipe as relay.js, but built on Deno's native WebSocket
// server so it runs on Deno Deploy: free, always-on, and wss:// out of the box
// (no TLS setup, no cold starts). It never parses the game protocol — Realm's
// lockstep handshake runs end-to-end inside the two browsers.
//
//   Local:        deno run --allow-net --allow-env relay_deno.ts
//                 -> ws://localhost:7523
//   Deno Deploy:  push this file to a repo and link it as the entrypoint,
//                 or `deployctl deploy --entrypoint relay_deno.ts`
//                 -> wss://<your-project>.deno.dev
//
// Clients connect to  <url>/?room=CODE&role=host|join   (see web-relay/README).

interface Room { host?: WebSocket; join?: WebSocket; }
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

  // Decide accept/reject synchronously so a fast joiner finds the host, but
  // only *send* the rejection once the socket is open.
  let reject: string | null = null;
  let room = rooms.get(code);
  if (!code || (role !== "host" && role !== "join")) {
    reject = "ERR bad request";
  } else if (role === "host") {
    if (room?.host && room.host.readyState === WebSocket.OPEN) {
      reject = "ERR room code already in use";
    } else {
      room = room ?? {};
      room.host = socket;
      rooms.set(code, room);
    }
  } else {
    if (!room || !room.host) reject = "ERR no such room (has your friend opened the lobby?)";
    else if (room.join) reject = "ERR room is full";
    else room.join = socket;
  }

  if (reject) {
    // Rejected sockets get no teardown handler, so they can't free a room
    // that isn't theirs.
    socket.onopen = () => { socket.send(reject!); socket.close(); };
    return response;
  }

  socket.onopen = () =>
    console.log(`[${code}] ${role} connected` + (room!.host && room!.join ? " — paired" : ""));

  socket.onmessage = (e) => {
    const r = rooms.get(code);
    if (!r) return;
    const peer = r.host === socket ? r.join : r.host;
    if (peer && peer.readyState === WebSocket.OPEN) peer.send(e.data);
  };

  const teardown = () => {
    const r = rooms.get(code);
    if (!r || (r.host !== socket && r.join !== socket)) return;   // not our room
    const peer = r.host === socket ? r.join : r.host;
    try { peer?.close(); } catch { /* already gone */ }
    rooms.delete(code);
    console.log(`[${code}] closed`);
  };
  socket.onclose = teardown;
  socket.onerror = teardown;

  return response;
}

const port = Number(Deno.env.get("PORT") ?? "7523");
Deno.serve({ port }, handle);
console.log(`Realm relay (Deno) listening on :${port}`);
