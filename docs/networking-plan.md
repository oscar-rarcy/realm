# Networking plan — lockstep LAN duel (roadmap step 5)

**STATUS: IMPLEMENTED (2026-07-02, net.cpp).** All six phases below shipped;
deltas from the plan: pause travels as a control message (both sims freeze at
the same tick anyway since bundles stop), LAN lobby discovery via UDP
broadcast on 7522 shipped alongside direct-IP join rather than "later",
and the desync alarm freezes into an on-screen banner (both replays are on
disk as planned). Headless harness: `./realm --net-host <ticks> [ais]` +
`./realm --net-join <addr> <ticks>` — run both, compare printed hashes.
Verified: 10k ticks / 2 AIs / scripted cross-wire commands, hash-identical.

Everything below builds on machinery that already exists and is verified:
deterministic sim (`--verify`), the command funnel, the replay
serialization format, and `simStateHash()`.

## Model

Classic deterministic lockstep, 2 humans (AIs allowed on top — they're
sim code and run identically on both machines). No game state ever
crosses the wire: only Commands, exactly the structs the replay file
already stores.

## Transport

TCP, one socket, non-blocking, polled from the match loop. Ordered +
reliable is precisely what lockstep wants, and at an 80ms tick the
latency cost of TCP on LAN is irrelevant. Plain BSD sockets — no
dependencies, fits the flat-code ethos. Direct IP first; discovery later.

## Phases

1. **`g.localPlayer` refactor** (the only wide change, all mechanical):
   input pushes commands as player 0, render draws fog `visible[0]`,
   statuses check `owner == 0` — every hardcoded 0 becomes
   `g.localPlayer`. Testable solo: start a match as slot 1 vs AIs and
   play it; if everything works, the refactor is complete.
2. **Shared command codec**: lift the field-by-field write/read in
   `replayRecord`/`replayReadNext` into `encodeCommand`/`decodeCommand`
   used by both the replay file and the socket. One format everywhere —
   a replay IS a recorded wire stream.
3. **Lobby**: splash gains [H]ost / [J]oin-IP. Handshake: magic +
   version; host sends seed, biomeChoice, numAIs, slot assignment
   (host 0, client 1). Both call initGame with the shared seed —
   identical worlds, zero map data transferred.
4. **Lockstep scheduler**: commands issued during tick T are scheduled
   for T+D (start D=3 ≈ 240ms). Every tick, each side sends its bundle
   for T+D — empty bundles included; they're the keepalive. simTick may
   advance past T only when both bundles for T have arrived; otherwise
   the loop blocks and shows "Waiting for opponent…". Apply order is
   fixed (slot 0's bundle, then slot 1's) so arrival order can't desync.
5. **Desync alarm**: piggyback `simStateHash()` in every 100th bundle.
   Mismatch → freeze both sides with "desync at tick N", write both
   replays to disk. The hash log + replay infra make diagnosis offline.
6. **Polish**: pause as a Command (both sides pause together);
   disconnect → surviving player chooses AI-takeover or victory;
   save/load disabled in MP v1; recording stays on (record BOTH
   players' streams → MP replays for free).

## Deliberately deferred

- UDP/relay/NAT traversal (LAN/direct IP covers the actual use case).
- Adaptive command delay (fixed D=3 until real latency data exists).
- Cross-architecture play (sim uses floats in dayPhase/projectiles —
  same-binary-same-arch is a documented constraint already).
- >2 humans (the scheduler generalises, the lobby UI doesn't need to yet).
