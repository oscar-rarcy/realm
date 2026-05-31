import { chromium } from 'playwright';
import { mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import path from 'node:path';

const baseUrl = process.env.REALM_WEB_URL || 'http://127.0.0.1:4173/';
const outDir = process.env.REALM_WEB_SCREENSHOT_DIR || 'build/web-render-checks';
const [viewportWidth, viewportHeight] = (process.env.REALM_WEB_VIEWPORT || '1280x820')
  .split('x')
  .map((part) => Number.parseInt(part, 10));

const common = new URLSearchParams({
  seed: '2468',
  corner: '1',
  ais: '1',
  biome: '0',
});

const cases = [
  { name: 'emoji-isometric', display: 'emoji', projection: 'isometric' },
  { name: 'emoji-topdown', display: 'emoji', projection: 'topdown' },
  { name: 'ascii-isometric', display: 'ascii', projection: 'isometric' },
  { name: 'ascii-topdown', display: 'ascii', projection: 'topdown' },
];

function hashBuffer(buffer) {
  return createHash('sha256').update(buffer).digest('hex').slice(0, 16);
}

function caseUrl(testCase) {
  const url = new URL(baseUrl);
  for (const [key, value] of common) url.searchParams.set(key, value);
  url.searchParams.set('display', testCase.display);
  url.searchParams.set('projection', testCase.projection);
  return url.toString();
}

async function canvasStats(page) {
  return page.evaluate(() => {
    const canvas = document.querySelector('canvas');
    const context = canvas.getContext('2d') || canvas.getContext('webgl') || canvas.getContext('webgl2');
    const dataUrl = canvas.toDataURL('image/png');
    let hash = 2166136261;
    for (let index = 0; index < dataUrl.length; index += 1) {
      hash ^= dataUrl.charCodeAt(index);
      hash = Math.imul(hash, 16777619) >>> 0;
    }
    const box = canvas.getBoundingClientRect();
    return {
      hash: hash.toString(16).padStart(8, '0'),
      dataUrlLength: dataUrl.length,
      width: canvas.width,
      height: canvas.height,
      clientWidth: Math.round(box.width),
      clientHeight: Math.round(box.height),
      hasContext: Boolean(context),
    };
  });
}

async function runCase(browser, testCase) {
  const page = await browser.newPage({
    viewport: { width: viewportWidth, height: viewportHeight },
    deviceScaleFactor: 1,
  });
  const messages = [];
  const failedRequests = [];
  const successfulResponses = new Set();
  const badResponses = [];

  page.on('console', (msg) => messages.push({ type: msg.type(), text: msg.text() }));
  page.on('pageerror', (err) => messages.push({ type: 'pageerror', text: err.message }));
  page.on('requestfailed', (request) => {
    const url = request.url();
    if (!url.includes('/favicon.ico')) {
      failedRequests.push({ url, failure: request.failure()?.errorText || '' });
    }
  });
  page.on('response', (response) => {
    if (response.status() < 400) successfulResponses.add(response.url());
    if (response.status() >= 400 && !response.url().includes('/favicon.ico')) {
      badResponses.push({ url: response.url(), status: response.status() });
    }
  });

  const url = caseUrl(testCase);
  await page.goto(url, { waitUntil: 'domcontentloaded' });
  await page.waitForSelector('canvas', { timeout: 30000 });
  await page.waitForFunction(() => globalThis.realmReady === true, null, { timeout: 60000 });
  await page.waitForFunction(() => {
    const module = globalThis.Module;
    return module && typeof module._realm_web_tick === 'function' && module._realm_web_tick() > 2;
  }, null, { timeout: 60000 });

  const screenshotPath = path.join(outDir, `${testCase.name}.png`);
  const screenshot = await page.screenshot({ path: screenshotPath, fullPage: false });

  const stats = await canvasStats(page);
  const screenshotHash = hashBuffer(screenshot);
  await page.locator('canvas').click({ position: { x: Math.floor(viewportWidth / 2), y: Math.floor(viewportHeight / 2) } });
  await page.waitForTimeout(100);
  const tick = await page.evaluate(() => globalThis.Module._realm_web_tick());
  const selectedBefore = await page.evaluate(() => globalThis.Module._realm_web_selected_id());
  await page.keyboard.press('F6');
  await page.waitForTimeout(250);
  const afterF6Hash = hashBuffer(await page.screenshot({ fullPage: false }));
  await page.keyboard.press('F7');
  await page.waitForTimeout(250);
  const afterF7Hash = hashBuffer(await page.screenshot({ fullPage: false }));
  await page.mouse.move(Math.floor(viewportWidth / 2), Math.floor(viewportHeight / 2));
  await page.mouse.wheel(0, -600);
  await page.waitForTimeout(250);
  const afterZoomHash = hashBuffer(await page.screenshot({ fullPage: false }));
  const selectedAfter = await page.evaluate(() => globalThis.Module._realm_web_selected_id());

  await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
  await page.waitForTimeout(500);
  await page.close();

  const actionableFailedRequests = failedRequests.filter((request) => {
    return !(request.url.endsWith('/index.data') && successfulResponses.has(request.url));
  });

  return {
    ...testCase,
    url,
    screenshotPath,
    tick,
    selectedBefore,
    selectedAfter,
    screenshotHash,
    stats,
    controls: {
      projectionKeysChangedFrame:
        afterF6Hash !== screenshotHash || afterF7Hash !== afterF6Hash,
      zoomChangedFrame: afterZoomHash !== afterF7Hash,
    },
    messages,
    failedRequests: actionableFailedRequests,
    benignAbortedRequests: failedRequests.filter((request) => !actionableFailedRequests.includes(request)),
    badResponses,
  };
}

await mkdir(outDir, { recursive: true });

const browser = await chromium.launch();
const results = [];
try {
  for (const testCase of cases) {
    results.push(await runCase(browser, testCase));
  }
} finally {
  await browser.close();
}

const failures = [];
for (const result of results) {
  if (result.failedRequests.length > 0) {
    failures.push(`${result.name}: failed requests ${JSON.stringify(result.failedRequests)}`);
  }
  if (result.badResponses.length > 0) {
    failures.push(`${result.name}: bad responses ${JSON.stringify(result.badResponses)}`);
  }
  if (!result.stats.hasContext || result.stats.width < 300 || result.stats.height < 200) {
    failures.push(`${result.name}: invalid canvas ${JSON.stringify(result.stats)}`);
  }
  if (result.stats.dataUrlLength < 2000) {
    failures.push(`${result.name}: canvas export is unexpectedly small`);
  }
  if (!result.controls.projectionKeysChangedFrame) {
    failures.push(`${result.name}: F6/F7 projection controls did not change the rendered frame`);
  }
  if (!result.controls.zoomChangedFrame) {
    failures.push(`${result.name}: mouse-wheel zoom did not change the rendered frame`);
  }

  const text = result.messages.map((message) => message.text).join('\n');
  if (/uncaught|exception|abort|content security|webassembly|wasm/i.test(text)) {
    failures.push(`${result.name}: fatal console output\n${text}`);
  }
  if (/No emoji font found/i.test(text)) {
    failures.push(`${result.name}: emoji font fallback was used`);
  }
  if (result.display === 'emoji' && !/Emoji font: \/assets\/fonts\/RealmSymbols\.ttf/i.test(text)) {
    failures.push(`${result.name}: bundled symbol font was not loaded`);
  }
}

const screenshotHashes = new Set(results.map((result) => result.screenshotHash));
if (screenshotHashes.size !== results.length) {
  failures.push('render mode screenshots were not visually distinct across all display/projection combinations');
}

console.log(JSON.stringify({ baseUrl, outDir, results }, null, 2));

if (failures.length > 0) {
  throw new Error(`Realm web render mode checks failed:\n${failures.join('\n')}`);
}

console.log(`Realm web render mode checks passed at ${baseUrl}`);
