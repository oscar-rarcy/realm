// relay.js — Realm web-multiplayer relay.
//
// A browser tab can't open a raw TCP socket or listen(), so web players
// can't reach each other directly. Everyone connects OUT to this relay,
// which groups them by a shared room code: one host plus up to MAX_JOIN
// challengers (4-player games). It is deliberately dumb — it never parses
// Realm's protocol, it only ROUTES bytes:
//   joiner -> host   each message is forwarded with the joiner's slot
//                    index prepended:  [idx][bytes]
//   host -> joiner   the host prepends the destination index; the relay
//                    strips it and forwards the bytes verbatim.
// The joiner side stays a plain pipe; all seat logic lives in the game.
//
//   node relay.js [port]              (default 7523, or $PORT)
//
// Each client connects to:
//   ws://<host>:<port>/?room=CODE&role=host|join
// Before pairing the relay may send ONE text control frame:
//   "ERR <reason>"   -> the game shows a friendly lobby message.
// The host additionally gets "BYE <idx>" when a joiner's socket closes.
// The room dies when the HOST leaves; a joiner leaving just frees its slot.

const http = require('http');
const { WebSocketServer } = require('ws');

const PORT = parseInt(process.argv[2] || process.env.PORT || '7523', 10);
const OPEN = 1; // ws.readyState === OPEN
const MAX_JOIN = 3; // matches MAX_NET_CLIENTS in the game

const rooms = new Map(); // code -> { host, joins: [ws|null x MAX_JOIN] }

// A plain HTTP GET doubles as a health check ("is the relay up?").
const server = http.createServer((req, res) => {
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end('Realm relay is up. Connect a WebSocket: /?room=CODE&role=host|join\n');
});

const wss = new WebSocketServer({ server, maxPayload: 1 << 20 });

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
  let idx = -1;
  if (role === 'host') {
    if (room && room.host && room.host.readyState === OPEN) {
      ws.send('ERR room code already in use');
      ws.close();
      return;
    }
    room = room || { host: null, joins: new Array(MAX_JOIN).fill(null) };
    room.host = ws;
    rooms.set(code, room);
  } else {
    if (!room || !room.host || room.host.readyState !== OPEN) {
      ws.send('ERR no such room (has your friend opened the lobby?)');
      ws.close();
      return;
    }
    idx = room.joins.findIndex(j => !j || j.readyState !== OPEN);
    if (idx < 0) {
      ws.send('ERR room is full');
      ws.close();
      return;
    }
    room.joins[idx] = ws;
  }
  console.log(`[${code}] ${role}${idx >= 0 ? idx : ''} connected`);

  ws.on('message', (data, isBinary) => {
    const r = rooms.get(code);
    if (!r || !isBinary) return;               // game traffic is binary-only
    if (ws === r.host) {
      if (data.length < 1) return;             // [destIdx][bytes]
      const d = data[0];
      const peer = d < MAX_JOIN ? r.joins[d] : null;
      if (peer && peer.readyState === OPEN) peer.send(data.subarray(1));
    } else {
      const i = r.joins.indexOf(ws);
      if (i < 0) return;
      if (r.host && r.host.readyState === OPEN)
        r.host.send(Buffer.concat([Buffer.from([i]), data]));
    }
  });

  const teardown = () => {
    const r = rooms.get(code);
    if (!r) return;
    if (ws === r.host) {
      for (const j of r.joins) if (j && j.readyState === OPEN) j.close();
      rooms.delete(code);
      console.log(`[${code}] closed`);
    } else {
      const i = r.joins.indexOf(ws);
      if (i < 0) return;
      r.joins[i] = null;
      if (r.host && r.host.readyState === OPEN) r.host.send(`BYE ${i}`);
      console.log(`[${code}] join${i} left`);
    }
  };
  ws.on('close', teardown);
  ws.on('error', teardown);
});

server.listen(PORT, () => {
  console.log(`Realm relay listening on :${PORT}`);
  console.log(`  clients connect to ws://<this-host>:${PORT}/?room=CODE&role=host|join`);
});
