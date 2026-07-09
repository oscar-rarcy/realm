# Realm web-multiplayer relay

Browser tabs can't open raw TCP or act as a server, so web players can't
reach each other directly. This tiny relay groups them by a shared **room
code** — one host plus up to three challengers (**4-player games**) — and
routes bytes between the host and each joiner (joiner messages reach the
host with a one-byte slot index prepended; the host prefixes a destination
index the relay strips). It never parses the game protocol — Realm's
lockstep handshake and per-tick command merging run entirely inside the
`.wasm` instances, so the relay stays dumb.

Only **web-vs-web** play is supported (browsers on the same build stay in
lockstep because they run byte-identical WebAssembly). Native desktop peers
use the built-in TCP path instead. A joiner who disconnects mid-match (or
whose tab the browser freezes for 10+ seconds) is dropped and the battle
carries on without them; the room dies only when the host leaves.

Three interchangeable implementations, one contract (`node test.js <url>`
verifies any of them): `relay.js` (Node), `worker.js` (Cloudflare Workers),
`relay_deno.ts` (Deno).

## Run it locally (for testing)

```sh
cd web-relay
npm install          # once, pulls in `ws`
node relay.js        # listens on ws://localhost:7523
```

Then serve the game and open two browsers:

```sh
python3 -m http.server 8080 -d ../web
```

In browser A: **MULTIPLAYER → HOST A GAME** (note the room code, relay is the
default `ws://localhost:7523`). In browser B: **MULTIPLAYER → JOIN A GAME**,
type the same room code and relay, connect. Host presses **Begin**.

## How the hosted page finds the relay

The web build picks its default relay at runtime, in this order:

1. `?relay=wss://…` query parameter on the page URL,
2. a `relay.json` served beside `index.html` — `{ "relay": "wss://…" }`,
3. the compile-time default (`make web RELAY_URL=wss://…`, else
   `ws://localhost:7523`).

So a deployed page (GitHub Pages, itch.io) can be re-pointed at a new relay by
editing one JSON file — no rebuild. Players can still override the URL in the
lobby's Relay field. Note: from an `https://` page the relay MUST be `wss://`
— browsers block plain `ws://` there.

## Play over the internet

### Cloudflare Workers (recommended — free, no card, always-on)

`worker.js` + `wrangler.toml` run the relay as a Cloudflare Worker with one
Durable Object per room (SQLite-backed class, so the **free plan** suffices;
WebSocket hibernation keeps idle lobbies free). From this folder:

```sh
npm i -g wrangler
wrangler login        # opens the browser; a free account needs no payment card
wrangler deploy       # -> https://realm-relay.<your-subdomain>.workers.dev
```

Use it in game as `wss://realm-relay.<your-subdomain>.workers.dev`. Free-tier
limits (100k requests/day) comfortably cover friends-scale play. Optional
hardening: set `ALLOWED_ORIGINS` in `wrangler.toml` so only your game page can
use the relay.

### Instant, zero-account: Cloudflare quick tunnel

For a play session right now, expose a local relay through a free
[TryCloudflare](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/do-more-with-tunnels/trycloudflare/)
tunnel — no account, no card:

```sh
node relay.js 7523                                  # terminal 1
cloudflared tunnel --url http://localhost:7523     # terminal 2 (brew install cloudflared)
```

`cloudflared` prints `https://<random>.trycloudflare.com`; the relay is then at
`wss://<random>.trycloudflare.com`. Caveats: it only lives while both processes
run on your machine, and the URL changes every start (update `relay.json` or
paste it in the lobby).

### Deno Deploy

`relay_deno.ts` is the same relay for [Deno Deploy](https://deno.com/deploy)
(free, always-on, TLS included) — but signups were returning
`SIGNUP_UNAVAILABLE` as of July 2026, so this path is parked. If you have a
working account: `deno run -A jsr:@deno/deployctl deploy --entrypoint
relay_deno.ts` → `wss://<project>.deno.dev`.

### Other options

- **Node host / VPS**: `node relay.js 7523` behind your firewall/NAT or on a
  cloud box (add a TLS terminator for `wss://`).
- **Render / Fly.io**: deploy `relay.js` as-is from a GitHub repo; both give an
  automatic `wss://` URL, but both ask for a payment card on signup.
- **Tailscale**: run the relay on one machine and use its Tailscale address —
  no port-forwarding, private, works between cities.

The Node/Deno relays use one port (default **7523**, override with an arg or
`$PORT`). A plain `GET /` on any implementation returns a health line so you
can check it's up in a browser.
