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
public relay URL in the lobby's **Relay** field:

- **Any small Node host / VPS**: `node relay.js 7523` behind your firewall/NAT,
  or on a cloud box. Put it behind TLS (`wss://`) if the page is served over
  HTTPS — browsers block mixed `ws://` from an `https://` page.
- **Tailscale**: run the relay on one machine and use its Tailscale address as
  the relay URL — no port-forwarding, works between cities.
- Build the URL into the client so friends don't have to type it:
  `make web RELAY_URL=wss://your.relay.example`.

The relay uses one TCP port (default **7523**, override with `node relay.js
<port>` or `$PORT`). A plain `GET /` returns a health line so you can check it's
up in a browser.
