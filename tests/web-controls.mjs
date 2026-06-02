import { mkdir } from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright';

const baseUrl = process.env.REALM_WEB_URL || 'http://127.0.0.1:4173/';
const outDir = process.env.REALM_WEB_CONTROL_OUT || path.join('build', 'web-control-checks');
const matchQuery = 'seed=2468&corner=1&ais=1&biome=0';

function routeUrl(segment) {
  const url = new URL(baseUrl);
  const [pathname, query = ''] = segment.split('?');
  const basePath = url.pathname.replace(/\/$/, '');
  url.pathname = `${basePath}/${pathname}`.replace(/\/+/g, '/');
  url.search = query ? `?${query}` : '';
  return url.toString();
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

async function waitReady(page) {
  await page.waitForSelector('canvas', { timeout: 30000 });
  await page.waitForFunction(() => globalThis.realmReady === true, null, { timeout: 60000 });
  await page.waitForFunction(() => {
    const module = globalThis.Module;
    return module
      && typeof module._realm_web_tick === 'function'
      && typeof module._realm_web_view_x === 'function'
      && typeof module._realm_web_screen_x_for_tile === 'function';
  }, null, { timeout: 60000 });
  await page.waitForFunction(() => globalThis.Module._realm_web_screen() === 1
    && globalThis.Module._realm_web_tick() > 5
    && globalThis.Module._realm_web_entity_count() > 0, null, { timeout: 60000 });
}

async function state(page) {
  return page.evaluate(() => ({
    tick: Module._realm_web_tick(),
    selectedId: Module._realm_web_selected_id(),
    selectedCount: Module._realm_web_selected_count(),
    viewX: Module._realm_web_view_x(),
    viewY: Module._realm_web_view_y(),
    viewW: Module._realm_web_view_w(),
    viewH: Module._realm_web_view_h(),
    cursorX: Module._realm_web_cursor_x(),
    cursorY: Module._realm_web_cursor_y(),
    unitX: Module._realm_web_first_owned_unit_x(),
    unitY: Module._realm_web_first_owned_unit_y(),
    displayMode: Module._realm_web_display_mode(),
    asciiOnly: Module._realm_web_ascii_only(),
    width: window.innerWidth,
    height: window.innerHeight,
  }));
}

async function screenForTile(page, x, y) {
  return page.evaluate(([mx, my]) => ({
    x: Module._realm_web_screen_x_for_tile(mx, my),
    y: Module._realm_web_screen_y_for_tile(mx, my),
  }), [x, y]);
}

async function runCase(browser, testCase) {
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
  const messages = [];
  page.on('console', (msg) => {
    if (msg.type() === 'error') messages.push(msg.text());
  });

  await page.goto(testCase.url, { waitUntil: 'domcontentloaded' });
  await waitReady(page);
  let current = await state(page);
  assert(current.displayMode === testCase.displayMode, `${testCase.name}: wrong display mode ${current.displayMode}`);
  assert(current.unitX >= 0 && current.unitY >= 0, `${testCase.name}: no owned unit found`);

  let unitScreen = await screenForTile(page, current.unitX, current.unitY);
  assert(unitScreen.x >= 0 && unitScreen.y >= 0, `${testCase.name}: owned unit is not visible`);
  await page.mouse.click(unitScreen.x, unitScreen.y);
  await page.waitForTimeout(200);
  current = await state(page);
  assert(current.selectedCount >= 1, `${testCase.name}: left-click did not select a unit`);

  const dragStart = await screenForTile(page, current.unitX - 2, current.unitY - 1);
  const dragEnd = await screenForTile(page, current.unitX + 5, current.unitY + 2);
  assert(dragStart.x >= 0 && dragEnd.x >= 0, `${testCase.name}: drag bounds are not visible`);
  await page.mouse.move(dragStart.x, dragStart.y);
  await page.mouse.down();
  await page.mouse.move(dragEnd.x, dragEnd.y, { steps: 10 });
  await page.waitForTimeout(100);
  await page.mouse.up();
  await page.waitForTimeout(250);
  current = await state(page);
  assert(current.selectedCount >= 1, `${testCase.name}: drag select did not keep/select units`);

  const target = { x: current.unitX, y: current.unitY };
  const targetScreen = await screenForTile(page, target.x, target.y);
  assert(targetScreen.x >= 0 && targetScreen.y >= 0, `${testCase.name}: command target is not visible`);
  await page.mouse.click(targetScreen.x, targetScreen.y, { button: 'right' });
  await page.waitForTimeout(250);
  current = await state(page);
  assert(current.cursorX === target.x && current.cursorY === target.y,
    `${testCase.name}: right-click command did not move cursor to target (${current.cursorX},${current.cursorY})`);

  const beforeZoom = current;
  await page.mouse.move(Math.floor(current.width / 2), Math.floor(current.height / 2));
  await page.mouse.wheel(0, -600);
  await page.waitForTimeout(350);
  current = await state(page);
  assert(current.viewW !== beforeZoom.viewW || current.viewH !== beforeZoom.viewH,
    `${testCase.name}: mouse wheel zoom did not change viewport size`);

  const beforePan = current;
  await page.mouse.move(Math.floor(current.width / 2), Math.floor(current.height / 2));
  await page.mouse.down({ button: 'middle' });
  await page.mouse.move(Math.floor(current.width / 2) + 180, Math.floor(current.height / 2) + 120, { steps: 12 });
  await page.waitForTimeout(100);
  await page.mouse.up({ button: 'middle' });
  await page.waitForTimeout(300);
  current = await state(page);
  assert(current.viewX !== beforePan.viewX || current.viewY !== beforePan.viewY,
    `${testCase.name}: middle-drag pan did not move viewport`);

  const beforeMini = current;
  await page.mouse.move(current.width - 60, 72);
  await page.mouse.down();
  await page.mouse.move(current.width - 55, 108, { steps: 4 });
  await page.mouse.up();
  await page.waitForTimeout(300);
  current = await state(page);
  assert(current.cursorX !== beforeMini.cursorX || current.cursorY !== beforeMini.cursorY
    || current.viewX !== beforeMini.viewX || current.viewY !== beforeMini.viewY,
    `${testCase.name}: minimap click/drag did not move viewport`);

  const screenshotPath = path.join(outDir, `${testCase.name}-controls.png`);
  await page.screenshot({ path: screenshotPath, fullPage: false });
  await page.close();
  return { name: testCase.name, screenshotPath, final: current, messages };
}

await mkdir(outDir, { recursive: true });

const cases = [
  { name: 'tileset', url: routeUrl(`embed?${matchQuery}`), displayMode: 1 },
  { name: 'ascii', url: routeUrl(`ascii/embed?${matchQuery}`), displayMode: 0 },
];

const browser = await chromium.launch();
const results = [];
try {
  for (const testCase of cases) results.push(await runCase(browser, testCase));
} finally {
  await browser.close();
}

const errors = results.flatMap((result) => result.messages.map((msg) => `${result.name}: ${msg}`));
if (errors.length > 0) {
  console.error(errors.join('\n'));
  process.exit(1);
}

console.log('Realm web controls passed');
console.log(JSON.stringify(results, null, 2));
