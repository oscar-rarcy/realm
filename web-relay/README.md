# Realm web-multiplayer relay

Browser tabs can't open raw TCP or act as a server, so two web players can't
reach each other directly. This tiny relay pairs them by a shared **room code**
and forwards raw bytes between the two browsers. It never parses the game
protocol — Realm's lockstep handshake runs end-to-end inside the two `.wasm`
instances, so the relay stays dumb and stateless-per-room.

Only **web-vs-web** play is supported (two browsers on the same build stay in
lockstep because they run byte-identical WebAssembly). Native desktop peers use
the built-in TCP path instead.

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

## Play over the internet

Host the relay anywhere reachable by both players and give both friends the same
public relay URL in the lobby's **Relay** field. If the game page is served over
HTTPS (GitHub Pages, itch.io), the relay MUST be `wss://` — browsers block a
plain `ws://` from an `https://` page.

### Deno Deploy (recommended — free, always-on, `wss://` built in)

`relay_deno.ts` is the same relay on Deno's native WebSocket server, ready for
[Deno Deploy](https://deno.com/deploy) (free tier, no cold starts, TLS included).

1. Install Deno: `curl -fsSL https://deno.land/install.sh | sh` (reopen your
   terminal afterwards so `deno` is on your PATH).
2. Deploy the relay from this folder — no separate install needed:
   `deno run -A jsr:@deno/deployctl deploy --entrypoint relay_deno.ts`
   (first run opens a browser to link your Deno account / project). If you'd
   rather install the `deployctl` command globally first, use
   `deno install -gArf jsr:@deno/deployctl` (Deno 2 needs the `-g`/`--global`
   flag) and then just run `deployctl deploy --entrypoint relay_deno.ts`.
   — or push the repo to GitHub and link the file in the Deno Deploy dashboard.
3. You get a URL like `wss://realm-relay.deno.dev`. Bake it into the build so
   friends don't type it: `make web RELAY_URL=wss://realm-relay.deno.dev`
   (they can still override it in the lobby's Relay field).

Run it locally the same way: `deno run --allow-net --allow-env relay_deno.ts`.

### Other options

- **Node host / VPS**: `node relay.js 7523` behind your firewall/NAT or on a
  cloud box (add a TLS terminator for `wss://`).
- **Render / Fly.io**: deploy `relay.js` as-is from a GitHub repo; both give an
  automatic `wss://` URL (Render's free tier cold-starts after idle).
- **Tailscale**: run the relay on one machine and use its Tailscale address —
  no port-forwarding, private, works between cities.

The relay uses one port (default **7523**, override with an arg or `$PORT`).
A plain `GET /` returns a health line so you can check it's up in a browser.
