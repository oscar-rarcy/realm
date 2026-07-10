// worker.js — Realm web-multiplayer relay on Cloudflare Workers.
//
// Same contract as relay.js: connect to  wss://<worker>/?room=CODE&role=host|join.
// One host plus up to MAX_JOIN challengers per room (8-player games). The
// relay never parses Realm's protocol — it only routes bytes:
//   joiner -> host   forwarded with the joiner's slot index prepended
//   host -> joiner   host prepends the destination index; relay strips it
// Control text frames: "ERR <reason>" before a rejection; the host gets
// "BYE <idx>" when a joiner's socket closes. The room dies with the host;
// a joiner leaving just frees its slot.
//
// One Durable Object instance per room code holds the sockets. The class is
// SQLite-backed (see wrangler.toml migrations) so it runs on the FREE plan,
// and it uses the WebSocket hibernation API so an idle lobby costs ~nothing.
//
// Deploy (free Cloudflare account, no card):
//   npm i -g wrangler
//   wrangler login
//   wrangler deploy        (from this folder)
//   -> https://realm-relay.<your-subdomain>.workers.dev   (use wss:// in game)

const MAX_FRAME = 256 * 1024; // Realm frames are tiny; anything huge is abuse
const MAX_CODE = 32;
const MAX_JOIN = 7;           // matches MAX_NET_CLIENTS in the game

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

  host() { return this.ctx.getWebSockets('host')[0] || null; }

  async fetch(req) {
    const role = new URL(req.url).searchParams.get('role');
    const pair = new WebSocketPair();
    if (role === 'host') {
      if (this.host()) return this.reject('room code already in use');
      this.ctx.acceptWebSocket(pair[1], ['host']);
    } else {
      if (!this.host()) return this.reject('no such room (has your friend opened the lobby?)');
      let idx = -1;
      for (let i = 0; i < MAX_JOIN; i++)
        if (!this.ctx.getWebSockets('j' + i).length) { idx = i; break; }
      if (idx < 0) return this.reject('room is full');
      this.ctx.acceptWebSocket(pair[1], ['join', 'j' + idx]);
    }
    return new Response(null, { status: 101, webSocket: pair[0] });
  }

  joinIndexOf(ws) {
    for (const t of this.ctx.getTags(ws))
      if (t.length === 2 && t[0] === 'j') return t.charCodeAt(1) - 48;
    return -1;
  }

  webSocketMessage(ws, msg) {
    if (typeof msg === 'string') return;       // game traffic is binary-only
    if (msg.byteLength > MAX_FRAME) { this.closeAll(); return; }
    const bytes = new Uint8Array(msg);
    if (this.ctx.getTags(ws).includes('host')) {
      if (bytes.length < 1) return;            // [destIdx][bytes]
      const peer = this.ctx.getWebSockets('j' + bytes[0])[0];
      if (peer) { try { peer.send(msg.slice(1)); } catch { /* mid-close */ } }
    } else {
      const i = this.joinIndexOf(ws);
      const h = this.host();
      if (i < 0 || !h) return;
      const out = new Uint8Array(1 + bytes.length);
      out[0] = i;
      out.set(bytes, 1);
      try { h.send(out); } catch { /* mid-close */ }
    }
  }

  webSocketClose(ws)  { this.gone(ws); }
  webSocketError(ws)  { this.gone(ws); }

  // Host leaving ends the room; a joiner leaving frees its slot and the
  // host is told so the game can stand that seat's forces down.
  gone(ws) {
    if (this.ctx.getTags(ws).includes('host')) { this.closeAll(); return; }
    const i = this.joinIndexOf(ws);
    const h = this.host();
    if (i >= 0 && h) { try { h.send('BYE ' + i); } catch { /* mid-close */ } }
  }

  closeAll() {
    for (const s of this.ctx.getWebSockets()) {
      try { s.close(1000, 'room closed'); } catch { /* already closed */ }
    }
  }
}
