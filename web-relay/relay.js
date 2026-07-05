// relay.js — Realm web-multiplayer relay.
//
// A browser tab can't open a raw TCP socket or listen() for a peer, so the
// two web players can't reach each other directly the way the native build
// does. Instead both browsers connect OUT to this relay, which pairs them by
// a shared room code and pipes raw bytes between them. It is deliberately
// dumb: it never parses Realm's protocol. The HELLO/WELCOME/START handshake
// and the entire lockstep scheduler run end-to-end inside the two .wasm
// instances exactly as they do over TCP natively.
//
//   node relay.js [port]              (default 7523, or $PORT)
//
// Each client connects to:
//   ws://<host>:<port>/?room=CODE&role=host|join
//     role=host  creates the room (fails if the code is already taken)
//     role=join  pairs into an existing room (fails if missing or full)
//
// Before pairing the relay may send ONE text control frame:
//   "ERR <reason>"   -> the game treats this as a lost connection and shows
//                       a friendly lobby message.
// Once both peers are present every binary frame is forwarded verbatim,
// host <-> join, until either side disconnects (which closes the peer too).

const http = require('http');
const { WebSocketServer } = require('ws');

const PORT = parseInt(process.argv[2] || process.env.PORT || '7523', 10);
const OPEN = 1; // ws.readyState === OPEN

const rooms = new Map(); // code -> { host, join }

// A plain HTTP GET doubles as a health check ("is the relay up?").
const server = http.createServer((req, res) => {
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end('Realm relay is up. Connect a WebSocket: /?room=CODE&role=host|join\n');
});

const wss = new WebSocketServer({ server });

const peerOf = (room, ws) => (ws === room.host ? room.join : room.host);

wss.on('connection', (ws, req) => {
  const url = new URL(req.url, 'http://localhost');
  const code = (url.searchParams.get('room') || '').trim();
  const role = url.searchParams.get('role');

  if (!code || (role !== 'host' && role !== 'join')) {
    ws.send('ERR bad request');
    ws.close();
    return;
  }

  let room = rooms.get(code);
  if (role === 'host') {
    if (room && room.host && room.host.readyState === OPEN) {
      ws.send('ERR room code already in use');
      ws.close();
      return;
    }
    room = room || {};
    room.host = ws;
    rooms.set(code, room);
  } else {
    if (!room || !room.host) {
      ws.send('ERR no such room (has your friend opened the lobby?)');
      ws.close();
      return;
    }
    if (room.join) {
      ws.send('ERR room is full');
      ws.close();
      return;
    }
    room.join = ws;
  }
  console.log(`[${code}] ${role} connected` + (room.host && room.join ? ' — paired' : ''));

  ws.on('message', (data, isBinary) => {
    const r = rooms.get(code);
    if (!r) return;
    const peer = peerOf(r, ws);
    if (peer && peer.readyState === OPEN) peer.send(data, { binary: isBinary });
  });

  const teardown = () => {
    const r = rooms.get(code);
    if (!r) return;
    const peer = peerOf(r, ws);
    if (peer && peer.readyState === OPEN) peer.close();
    rooms.delete(code);
    console.log(`[${code}] closed`);
  };
  ws.on('close', teardown);
  ws.on('error', teardown);
});

server.listen(PORT, () => {
  console.log(`Realm relay listening on :${PORT}`);
  console.log(`  clients connect to ws://<this-host>:${PORT}/?room=CODE&role=host|join`);
});
