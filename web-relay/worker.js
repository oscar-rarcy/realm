// worker.js — Realm web-multiplayer relay on Cloudflare Workers.
//
// Same contract as relay.js: connect to  wss://<worker>/?room=CODE&role=host|join.
// Before pairing the relay may send ONE text frame "ERR <reason>"; once both
// peers are present every frame is forwarded verbatim, and either side
// disconnecting closes the other. It never parses Realm's protocol — the
// lockstep handshake runs end-to-end inside the two .wasm instances.
//
// One Durable Object instance per room code holds the two sockets. The class
// is SQLite-backed (see wrangler.toml migrations) so it runs on the FREE plan,
// and it uses the WebSocket hibernation API so an idle lobby costs ~nothing.
//
// Deploy (free Cloudflare account, no card):
//   npm i -g wrangler
//   wrangler login
//   wrangler deploy        (from this folder)
//   -> https://realm-relay.<your-subdomain>.workers.dev   (use wss:// in game)

const MAX_FRAME = 256 * 1024; // Realm frames are tiny; anything huge is abuse
const MAX_CODE = 32;

export default {
  async fetch(req, env) {
    if (req.headers.get('Upgrade') !== 'websocket') {
      return new Response(
        'Realm relay is up. Connect a WebSocket: /?room=CODE&role=host|join\n',
        { headers: { 'content-type': 'text/plain' } });
    }

    // Optional origin pinning: set ALLOWED_ORIGINS = "https://a.com,https://b.com"
    // in wrangler.toml [vars] to refuse browsers on other sites. Unset = open.
    const allowed = (env.ALLOWED_ORIGINS || '')
      .split(',').map(s => s.trim()).filter(Boolean);
    if (allowed.length && !allowed.includes(req.headers.get('Origin') || ''))
      return new Response('origin not allowed', { status: 403 });

    const url = new URL(req.url);
    const code = (url.searchParams.get('room') || '').trim();
    const role = url.searchParams.get('role');
    if (!code || code.length > MAX_CODE || (role !== 'host' && role !== 'join'))
      return new Response('bad request', { status: 400 });

    // Every socket for a given code lands in the same DO instance.
    return env.ROOMS.get(env.ROOMS.idFromName(code)).fetch(req);
  }
};

export class RelayRoom {
  constructor(ctx) { this.ctx = ctx; }

  // Reject without entering the room: a plain (non-hibernating) accept so the
  // socket never joins the tagged set, one ERR text frame, then close.
  reject(reason) {
    const pair = new WebSocketPair();
    pair[1].accept();
    pair[1].send('ERR ' + reason);
    pair[1].close(1008, reason);
    return new Response(null, { status: 101, webSocket: pair[0] });
  }

  async fetch(req) {
    const role = new URL(req.url).searchParams.get('role');
    const hosts = this.ctx.getWebSockets('host');
    if (role === 'host' && hosts.length)
      return this.reject('room code already in use');
    if (role === 'join' && !hosts.length)
      return this.reject('no such room (has your friend opened the lobby?)');
    if (role === 'join' && this.ctx.getWebSockets('join').length)
      return this.reject('room is full');

    const pair = new WebSocketPair();
    this.ctx.acceptWebSocket(pair[1], [role]); // hibernation API
    return new Response(null, { status: 101, webSocket: pair[0] });
  }

  webSocketMessage(ws, msg) {
    const size = typeof msg === 'string' ? msg.length : msg.byteLength;
    if (size > MAX_FRAME) { this.closeRoom(); return; }
    const other = this.ctx.getTags(ws).includes('host') ? 'join' : 'host';
    for (const peer of this.ctx.getWebSockets(other)) {
      try { peer.send(msg); } catch { /* peer mid-close */ }
    }
  }

  webSocketClose() { this.closeRoom(); }
  webSocketError() { this.closeRoom(); }

  // Mirror relay.js: the room dies with either side, freeing the code.
  closeRoom() {
    for (const s of this.ctx.getWebSockets()) {
      try { s.close(1000, 'peer left'); } catch { /* already closed */ }
    }
  }
}
