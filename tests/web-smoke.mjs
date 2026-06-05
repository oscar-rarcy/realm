import { chromium } from 'playwright';

const url = process.env.REALM_WEB_URL || 'http://127.0.0.1:4173/';
const [viewportWidth, viewportHeight] = (process.env.REALM_WEB_VIEWPORT || '1280x820')
  .split('x')
  .map((part) => Number.parseInt(part, 10));
const deviceScaleFactor = Number.parseFloat(process.env.REALM_WEB_DEVICE_SCALE_FACTOR || '1');
const isMobile = process.env.REALM_WEB_IS_MOBILE === '1';
const expectedAsciiOnly = process.env.REALM_EXPECT_ASCII_ONLY === '1';
const browser = await chromium.launch();
const page = await browser.newPage({
  viewport: { width: viewportWidth, height: viewportHeight },
  deviceScaleFactor,
  isMobile,
  hasTouch: isMobile,
});

const errors = [];

function routeUrl(base, segment) {
  const routed = new URL(base);
  const [pathname, query = ''] = segment.split('?');
  routed.pathname = `${routed.pathname.replace(/\/$/, '')}/${pathname}`;
  routed.search = query ? `?${query}` : '';
  return routed.toString();
}

async function waitReady(testPage) {
  await testPage.waitForSelector('canvas', { timeout: 30000 });
  await testPage.waitForFunction(() => globalThis.realmReady === true, null, { timeout: 60000 });
  await testPage.waitForFunction(() => {
    const module = globalThis.Module;
    return module && typeof module._realm_web_tick === 'function'
      && typeof module._realm_web_screen === 'function'
      && typeof module._realm_web_ascii_only === 'function'
      && typeof module._realm_web_display_mode === 'function';
  }, null, { timeout: 60000 });
}

async function assertCanvasReady(testPage) {
  const canvasBox = await testPage.locator('canvas').boundingBox();
  if (!canvasBox || canvasBox.width < 300 || canvasBox.height < 200) {
    throw new Error(`canvas too small: ${JSON.stringify(canvasBox)}`);
  }

  const viewportFit = await testPage.evaluate(() => {
    const canvas = document.querySelector('canvas');
    const box = canvas.getBoundingClientRect();
    return {
      widthDelta: Math.abs(box.width - window.innerWidth),
      heightDelta: Math.abs(box.height - window.innerHeight),
      headerCount: document.querySelectorAll('header').length,
    };
  });
  if (viewportFit.widthDelta > 2 || viewportFit.heightDelta > 2) {
    throw new Error(`canvas does not fill viewport: ${JSON.stringify(viewportFit)}`);
  }
  if (viewportFit.headerCount !== 0) {
    throw new Error('unexpected web header rendered');
  }

  const pixelCheck = await testPage.evaluate(() => {
    const canvas = document.querySelector('canvas');
    return canvas.toDataURL('image/png').length;
  });
  if (pixelCheck < 2000) throw new Error('canvas export is unexpectedly small');
}

async function readState(testPage) {
  return testPage.evaluate(() => ({
    screen: globalThis.Module._realm_web_screen(),
    tick: globalThis.Module._realm_web_tick(),
    entities: globalThis.Module._realm_web_entity_count(),
    asciiOnly: globalThis.Module._realm_web_ascii_only(),
    displayMode: globalThis.Module._realm_web_display_mode(),
    fullscreenHook: typeof globalThis.realmToggleFullscreen === 'function',
  }));
}

page.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});
page.on('pageerror', (err) => errors.push(err.message));

await page.goto(url, { waitUntil: 'domcontentloaded' });
await waitReady(page);
await page.waitForTimeout(500);
await assertCanvasReady(page);

const menuState = await readState(page);
if (menuState.screen !== 0 || menuState.tick !== 0 || menuState.entities !== 0) {
  throw new Error(`main route should idle on the menu before player input: ${JSON.stringify(menuState)}`);
}
if (expectedAsciiOnly) {
  if (menuState.asciiOnly !== 1 || menuState.displayMode !== 0) {
    throw new Error(`main route should use the ASCII-only profile: ${JSON.stringify(menuState)}`);
  }
} else if (menuState.asciiOnly !== 0 || menuState.displayMode !== 1) {
  throw new Error(`main route should allow the tileset-capable menu: ${JSON.stringify(menuState)}`);
}

await page.locator('canvas').click({ position: { x: Math.floor(viewportWidth / 2), y: Math.floor(viewportHeight / 2) } });
await page.keyboard.press('Enter');
await page.waitForFunction(() => {
  const module = globalThis.Module;
  return module._realm_web_screen() === 1 && module._realm_web_tick() > 2 && module._realm_web_entity_count() > 0;
}, null, { timeout: 60000 });
const startedState = await readState(page);

const embedPage = await browser.newPage({
  viewport: { width: viewportWidth, height: viewportHeight },
  deviceScaleFactor,
  isMobile,
  hasTouch: isMobile,
});
embedPage.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});
embedPage.on('pageerror', (err) => errors.push(err.message));
const embedUrl = routeUrl(url, 'embed');
await embedPage.goto(embedUrl, { waitUntil: 'domcontentloaded' });
await waitReady(embedPage);
await embedPage.waitForFunction(() => {
  const module = globalThis.Module;
  return module._realm_web_screen() === 1 && module._realm_web_tick() > 2 && module._realm_web_entity_count() > 0;
}, null, { timeout: 60000 });
await assertCanvasReady(embedPage);
const embedState = await readState(embedPage);
if (!embedState.fullscreenHook) {
  throw new Error('web fullscreen hook was not installed');
}

await embedPage.locator('canvas').click({ position: { x: Math.floor(viewportWidth / 2), y: Math.floor(viewportHeight / 2) } });
await embedPage.keyboard.press('x');
await embedPage.waitForTimeout(500);
const afterXState = await readState(embedPage);
if (afterXState.screen !== 1 || afterXState.tick <= embedState.tick || afterXState.entities <= 0) {
  throw new Error(`X should not exit the web match: ${JSON.stringify({ embedState, afterXState })}`);
}

await embedPage.keyboard.press('q');
await embedPage.waitForFunction(() => globalThis.Module._realm_web_screen() === 0, null, { timeout: 60000 });
const afterQState = await readState(embedPage);
if (afterQState.screen !== 0) {
  throw new Error(`Q should resign to the web menu: ${JSON.stringify(afterQState)}`);
}

const queryOnlyEmbedPage = await browser.newPage({
  viewport: { width: viewportWidth, height: viewportHeight },
  deviceScaleFactor,
  isMobile,
  hasTouch: isMobile,
});
queryOnlyEmbedPage.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});
queryOnlyEmbedPage.on('pageerror', (err) => errors.push(err.message));
const queryOnlyEmbedUrl = routeUrl(url, '?embed=1');
await queryOnlyEmbedPage.goto(queryOnlyEmbedUrl, { waitUntil: 'domcontentloaded' });
await waitReady(queryOnlyEmbedPage);
await queryOnlyEmbedPage.waitForTimeout(500);
await assertCanvasReady(queryOnlyEmbedPage);
const queryOnlyEmbedState = await readState(queryOnlyEmbedPage);
if (queryOnlyEmbedState.screen !== 0 || queryOnlyEmbedState.tick !== 0 || queryOnlyEmbedState.entities !== 0) {
  throw new Error(`query-only embed must not start embed mode: ${JSON.stringify(queryOnlyEmbedState)}`);
}
const queryOnlyShellMode = await queryOnlyEmbedPage.evaluate(() => document.documentElement.dataset.realmShell);
if (queryOnlyShellMode !== 'menu') {
  throw new Error(`query-only embed shell should stay in menu mode, got ${queryOnlyShellMode}`);
}
await queryOnlyEmbedPage.close();

const asciiPage = await browser.newPage({
  viewport: { width: viewportWidth, height: viewportHeight },
  deviceScaleFactor,
  isMobile,
  hasTouch: isMobile,
});
asciiPage.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});
asciiPage.on('pageerror', (err) => errors.push(err.message));
const asciiUrl = routeUrl(url, 'ascii');
await asciiPage.goto(asciiUrl, { waitUntil: 'domcontentloaded' });
await waitReady(asciiPage);
await asciiPage.waitForTimeout(500);
await assertCanvasReady(asciiPage);
const asciiMenuState = await readState(asciiPage);
if (asciiMenuState.asciiOnly !== 1 || asciiMenuState.displayMode !== 0 || asciiMenuState.screen !== 0) {
  throw new Error(`ASCII route should open as ASCII-only menu: ${JSON.stringify(asciiMenuState)}`);
}
await asciiPage.keyboard.press('5');
await asciiPage.waitForTimeout(250);
const asciiAfterTilesetKey = await readState(asciiPage);
if (asciiAfterTilesetKey.displayMode !== 0) {
  throw new Error(`ASCII route should ignore the tileset selector: ${JSON.stringify(asciiAfterTilesetKey)}`);
}
await asciiPage.close();

if (errors.some((line) => /uncaught|exception|abort|content security|webassembly|wasm/i.test(line))) {
  throw new Error(`fatal browser console errors:\n${errors.join('\n')}`);
}

await embedPage.close();
await browser.close();
console.log(`Realm web smoke passed at ${url}`);
console.log(JSON.stringify({ menuState, startedState, embedUrl, embedState, afterXState, afterQState, queryOnlyEmbedUrl, queryOnlyEmbedState, asciiUrl, asciiMenuState, asciiAfterTilesetKey }, null, 2));
