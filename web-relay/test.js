// test.js — relay contract test. Runs against ANY Realm relay implementation
// (relay.js, relay_deno.ts, worker.js under `wrangler dev`, or a deployed URL):
//
//   node test.js [ws://localhost:7523]
//
// Verifies the room-code byte-pipe the game depends on: pair + verbatim
// binary pipe both ways, dup-host reject, join-before-host reject, full-room
// reject, and peer-close propagation. Exit 0 = all pass.
const WebSocket = require('ws');
const BASE = (process.argv[2] || 'ws://localhost:7523').replace(/\/$/, '');
const RUN = Date.now().toString(36); // unique room codes per run

let failures = 0;
const ok = (cond, name) => {
  console.log((cond ? 'PASS' : 'FAIL') + '  ' + name);
  if (!cond) failures++;
};

// Open a socket and record everything that happens to it.
function open(room, role) {
  const ws = new WebSocket(`${BASE}/?room=${room}-${RUN}&role=${role}`);
  const s = { ws, msgs: [], closed: false };
  ws.on('message', (data, isBinary) =>
    s.msgs.push(isBinary ? Buffer.from(data) : data.toString()));
  ws.on('close', () => { s.closed = true; });
  ws.on('error', () => { s.closed = true; });
  return s;
}
const wait = (ms) => new Promise(r => setTimeout(r, ms));
const firstText = (s) => s.msgs.find(m => typeof m === 'string') || '';

(async () => {
  // 1. Pair and pipe verbatim, both directions.
  const host = open('r1', 'host');
  await wait(600);
  const join = open('r1', 'join');
  await wait(600);
  host.ws.send(Buffer.from([1, 2, 3, 250]));
  join.ws.send(Buffer.from([9, 8]));
  await wait(600);
  const h2j = join.msgs.find(Buffer.isBuffer);
  const j2h = host.msgs.find(Buffer.isBuffer);
  ok(h2j && h2j.equals(Buffer.from([1, 2, 3, 250])), 'host->join bytes verbatim');
  ok(j2h && j2h.equals(Buffer.from([9, 8])), 'join->host bytes verbatim');

  // 2. Second host on a live room is turned away.
  const dup = open('r1', 'host');
  await wait(600);
  ok(firstText(dup).startsWith('ERR') && dup.closed, 'dup host rejected: ' + firstText(dup));

  // 3. Joining a room nobody hosts is turned away.
  const lost = open('nobody', 'join');
  await wait(600);
  ok(firstText(lost).startsWith('ERR') && lost.closed, 'no-room join rejected: ' + firstText(lost));

  // 4. Third wheel on a paired room is turned away.
  const third = open('r1', 'join');
  await wait(600);
  ok(firstText(third).startsWith('ERR') && third.closed, 'full room rejected: ' + firstText(third));

  // 5. Either side leaving closes the peer (game shows "lost connection").
  host.ws.close();
  await wait(800);
  ok(join.closed, 'host close propagates to joiner');

  // 6. The code is free again afterwards: a new host may reuse it.
  const re = open('r1', 'host');
  await wait(600);
  ok(!re.closed && !firstText(re).startsWith('ERR'), 'room code reusable after close');
  re.ws.close();

  await wait(200);
  console.log(failures ? `${failures} FAILURE(S)` : 'ALL PASS');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.log('THREW', e.message); process.exit(1); });
