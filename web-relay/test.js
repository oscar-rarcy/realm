// test.js — relay contract test. Runs against ANY Realm relay implementation
// (relay.js, relay_deno.ts, worker.js under `wrangler dev`, or a deployed URL):
//
//   node test.js [ws://localhost:7523]
//
// Verifies the 8-player room-code hub the game depends on: index-byte
// routing between the host and up to seven joiners, the reject reasons,
// BYE notifications when a joiner leaves, slot reuse, and the room dying
// with the host. Exit 0 = all pass.
const WebSocket = require('ws');
const BASE = (process.argv[2] || 'ws://localhost:7523').replace(/\/$/, '');
const RUN = Date.now().toString(36); // unique room codes per run

let failures = 0;
const ok = (cond, name) => {
  console.log((cond ? 'PASS' : 'FAIL') + '  ' + name);
  if (!cond) failures++;
};
const wait = (ms) => new Promise(r => setTimeout(r, ms));

// Open a socket and record everything that happens to it.
function open(room, role) {
  const ws = new WebSocket(`${BASE}/?room=${room}-${RUN}&role=${role}`);
  const s = { ws, bin: [], text: [], closed: false };
  ws.on('message', (data, isBinary) => {
    if (isBinary) s.bin.push(Buffer.from(data));
    else s.text.push(data.toString());
  });
  ws.on('close', () => { s.closed = true; });
  ws.on('error', () => { s.closed = true; });
  return s;
}
const firstText = (s) => s.text[0] || '';

(async () => {
  // 1. Host + three joiners, routed both ways with index bytes.
  const host = open('r1', 'host');
  await wait(600);
  const j0 = open('r1', 'join');
  const j1 = open('r1', 'join');
  const j2 = open('r1', 'join');
  await wait(700);
  j0.ws.send(Buffer.from([10, 11]));
  j1.ws.send(Buffer.from([20]));
  j2.ws.send(Buffer.from([30, 31, 32]));
  host.ws.send(Buffer.from([1, 99, 98]));       // -> j1 only, prefix stripped
  await wait(700);
  const from0 = host.bin.find(b => b[0] === 0);
  const from1 = host.bin.find(b => b[0] === 1);
  const from2 = host.bin.find(b => b[0] === 2);
  ok(from0 && from0.equals(Buffer.from([0, 10, 11])), 'join0 -> host tagged 0');
  ok(from1 && from1.equals(Buffer.from([1, 20])), 'join1 -> host tagged 1');
  ok(from2 && from2.equals(Buffer.from([2, 30, 31, 32])), 'join2 -> host tagged 2');
  ok(j1.bin.length === 1 && j1.bin[0].equals(Buffer.from([99, 98])), 'host -> join1 verbatim');
  ok(j0.bin.length === 0 && j2.bin.length === 0, 'host -> join1 reached ONLY join1');

  // 2. The room seats seven joiners; the highest slot still routes, and
  //    the eighth joiner is turned away.
  const extra = [open('r1', 'join'), open('r1', 'join'),
                 open('r1', 'join'), open('r1', 'join')];   // slots 3..6
  await wait(700);
  extra[3].ws.send(Buffer.from([60]));
  await wait(600);
  const from6 = host.bin.find(b => b[0] === 6);
  ok(from6 && from6.equals(Buffer.from([6, 60])), 'join6 -> host tagged 6');
  const j7 = open('r1', 'join');
  await wait(600);
  ok(firstText(j7).startsWith('ERR') && j7.closed, 'eighth join rejected: ' + firstText(j7));

  // 3. A second host on a live room is turned away.
  const dup = open('r1', 'host');
  await wait(600);
  ok(firstText(dup).startsWith('ERR') && dup.closed, 'dup host rejected: ' + firstText(dup));

  // 4. Joining a room nobody hosts is turned away.
  const lost = open('nobody', 'join');
  await wait(600);
  ok(firstText(lost).startsWith('ERR') && lost.closed, 'no-room join rejected: ' + firstText(lost));

  // 5. A joiner leaving frees its slot: host hears BYE, room lives on,
  //    and the next joiner gets the freed index back.
  j1.ws.close();
  await wait(700);
  ok(host.text.includes('BYE 1'), 'host notified: BYE 1');
  ok(!host.closed && !j0.closed && !j2.closed, 'room survives a joiner leaving');
  const j1b = open('r1', 'join');
  await wait(600);
  j1b.ws.send(Buffer.from([44]));
  await wait(600);
  ok(host.bin.some(b => b.equals(Buffer.from([1, 44]))), 'freed slot 1 reused');

  // 6. The host leaving closes everyone and frees the code.
  host.ws.close();
  await wait(800);
  ok(j0.closed && j2.closed && j1b.closed && extra.every(j => j.closed),
     'host close propagates to all joiners');
  const re = open('r1', 'host');
  await wait(600);
  ok(!re.closed && !firstText(re).startsWith('ERR'), 'room code reusable after close');
  re.ws.close();

  await wait(200);
  console.log(failures ? `${failures} FAILURE(S)` : 'ALL PASS');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.log('THREW', e.message); process.exit(1); });
