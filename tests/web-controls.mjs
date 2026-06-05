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
      && typeof module._realm_web_screen_x_for_tile === 'function'
      && typeof module._realm_web_selected_target_x === 'function'
      && typeof module._realm_web_minimap_x_for_screen === 'function';
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
    selectedTargetX: Module._realm_web_selected_target_x(),
    selectedTargetY: Module._realm_web_selected_target_y(),
    unitX: Module._realm_web_first_owned_unit_x(),
    unitY: Module._realm_web_first_owned_unit_y(),
    displayMode: Module._realm_web_display_mode(),
    asciiOnly: Module._realm_web_ascii_only(),
    width: window.innerWidth,
    height: window.innerHeight,
    minimapProbeX: Module._realm_web_minimap_x_for_screen(window.innerWidth - 60, 72),
    minimapProbeY: Module._realm_web_minimap_y_for_screen(window.innerWidth - 60, 72),
  }));
}

async function screenForTile(page, x, y) {
  return page.evaluate(([mx, my]) => ({
    x: Module._realm_web_screen_x_for_tile(mx, my),
    y: Module._realm_web_screen_y_for_tile(mx, my),
  }), [x, y]);
}

async function minimapPointNear(page, preferred) {
  return page.evaluate(([preferredX, preferredY]) => {
    function mapped(px, py) {
      const mx = Module._realm_web_minimap_x_for_screen(px, py);
      const my = Module._realm_web_minimap_y_for_screen(px, py);
      return mx >= 0 && my >= 0 ? { x: px, y: py, mapX: mx, mapY: my } : null;
    }

    const first = mapped(preferredX, preferredY);
    if (first) return first;

    for (let radius = 8; radius <= 260; radius += 8) {
      for (let dy = -radius; dy <= radius; dy += 8) {
        for (let dx = -radius; dx <= radius; dx += 8) {
          if (Math.abs(dx) !== radius && Math.abs(dy) !== radius) continue;
          const candidate = mapped(preferredX + dx, preferredY + dy);
          if (candidate) return candidate;
        }
      }
    }
    return null;
  }, [preferred.x, preferred.y]);
}

async function runParentEmbedCase(browser) {
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
  const iframeSrc = routeUrl(`ascii/embed?${matchQuery}`);
  await page.setContent(`<!doctype html>
    <html>
      <head>
        <style>
          body { margin: 0; height: 2400px; background: #f4f1e8; }
          .spacer { height: 360px; }
          iframe { display: block; width: 820px; height: 620px; margin: 0 auto; border: 0; }
        </style>
      </head>
      <body>
        <div class="spacer"></div>
        <iframe id="realm-frame" src="${iframeSrc}"></iframe>
      </body>
    </html>`);
  await page.evaluate(() => window.scrollTo(0, 260));
  const frameElement = await page.waitForSelector('#realm-frame', { timeout: 30000 });
  const frame = await frameElement.contentFrame();
  assert(frame, 'parent-embed: Realm iframe did not attach');
  await waitReady(frame);
  await frame.evaluate(() => {
    window.__realmMiddleDefaults = [];
    for (const eventName of ['mousedown', 'mouseup', 'auxclick']) {
      document.addEventListener(eventName, (event) => {
        if (event.button === 1 || (event.buttons & 4) === 4) {
          window.__realmMiddleDefaults.push({ type: event.type, defaultPrevented: event.defaultPrevented });
        }
      });
    }
  });

  const box = await frameElement.boundingBox();
  assert(box && box.width > 300 && box.height > 300, 'parent-embed: Realm iframe is not visible');
  const beforeScrollY = await page.evaluate(() => window.scrollY);
  const before = await state(frame);
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'middle' });
  await page.mouse.move(box.x + box.width / 2 + 160, box.y + box.height / 2 + 120, { steps: 10 });
  await page.waitForTimeout(100);
  await page.mouse.up({ button: 'middle' });
  await page.waitForTimeout(350);
  const after = await state(frame);
  const afterScrollY = await page.evaluate(() => window.scrollY);
  const defaultEvents = await frame.evaluate(() => window.__realmMiddleDefaults);
  await page.close();

  assert(after.viewX !== before.viewX || after.viewY !== before.viewY,
    'parent-embed: middle-drag did not pan the Realm viewport');
  assert(afterScrollY === beforeScrollY,
    `parent-embed: outer page scrolled during middle-drag (${beforeScrollY} -> ${afterScrollY})`);
  assert(defaultEvents.some((event) => event.type === 'mousedown' && event.defaultPrevented),
    `parent-embed: middle mousedown default was not prevented (${JSON.stringify(defaultEvents)})`);
  assert(defaultEvents.every((event) => event.defaultPrevented),
    `parent-embed: a middle-button event was not defaultPrevented (${JSON.stringify(defaultEvents)})`);

  return { name: 'parent-embed', beforeScrollY, afterScrollY, before, after, defaultEvents };
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

  const beforeEdgeHover = current;
  const edgeHoverPoints = [
    { name: 'right', x: current.width - 2, y: Math.floor(current.height / 2) },
    { name: 'left', x: 2, y: Math.floor(current.height / 2) },
    { name: 'top', x: Math.floor(current.width / 2), y: 2 },
    { name: 'bottom', x: Math.floor(current.width / 2), y: current.height - 2 },
  ];
  for (const point of edgeHoverPoints) {
    await page.mouse.move(point.x, point.y);
    await page.waitForTimeout(500);
    current = await state(page);
    assert(current.viewX === beforeEdgeHover.viewX && current.viewY === beforeEdgeHover.viewY,
      `${testCase.name}: passive ${point.name} edge hover moved viewport (${beforeEdgeHover.viewX},${beforeEdgeHover.viewY}) -> (${current.viewX},${current.viewY})`);
  }

  const minimapAvailable = current.minimapProbeX >= 0 && current.minimapProbeY >= 0;
  if (!minimapAvailable) {
    const screenshotPath = path.join(outDir, `${testCase.name}-controls.png`);
    await page.screenshot({ path: screenshotPath, fullPage: false });
    await page.close();
    return { name: testCase.name, screenshotPath, final: current, minimapAvailable, messages };
  }

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

  const middleMiniStart = await minimapPointNear(page, { x: current.width - 62, y: 84 });
  const middleMiniEnd = await minimapPointNear(page, { x: current.width - 58, y: 124 });
  assert(middleMiniStart && middleMiniEnd,
    `${testCase.name}: middle minimap drag end is not inside minimap`);
  await page.mouse.move(middleMiniStart.x, middleMiniStart.y);
  await page.mouse.down({ button: 'middle' });
  await page.mouse.move(middleMiniEnd.x, middleMiniEnd.y, { steps: 5 });
  await page.mouse.up({ button: 'middle' });
  await page.waitForTimeout(300);
  current = await state(page);
  assert(current.cursorX === middleMiniEnd.mapX && current.cursorY === middleMiniEnd.mapY,
    `${testCase.name}: middle-drag minimap did not move cursor to minimap target (${current.cursorX},${current.cursorY})`);

  const miniMovePoint = await minimapPointNear(page, { x: current.width - 72, y: 118 });
  assert(miniMovePoint,
    `${testCase.name}: minimap move point is not inside minimap`);
  await page.mouse.click(miniMovePoint.x, miniMovePoint.y, { button: 'right' });
  await page.waitForTimeout(350);
  current = await state(page);
  assert(current.cursorX === miniMovePoint.mapX && current.cursorY === miniMovePoint.mapY,
    `${testCase.name}: right-click minimap did not move cursor to minimap target`);
  assert(current.selectedTargetX === miniMovePoint.mapX && current.selectedTargetY === miniMovePoint.mapY,
    `${testCase.name}: right-click minimap did not issue move-only target (${current.selectedTargetX},${current.selectedTargetY})`);

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
let parentEmbedResult = null;
try {
  for (const testCase of cases) results.push(await runCase(browser, testCase));
  parentEmbedResult = await runParentEmbedCase(browser);
} finally {
  await browser.close();
}

const errors = results.flatMap((result) => result.messages.map((msg) => `${result.name}: ${msg}`));
if (errors.length > 0) {
  console.error(errors.join('\n'));
  process.exit(1);
}

console.log('Realm web controls passed');
console.log(JSON.stringify([...results, parentEmbedResult], null, 2));
