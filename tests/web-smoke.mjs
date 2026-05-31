import { chromium } from 'playwright';

const url = process.env.REALM_WEB_URL || 'http://127.0.0.1:4173/';
const [viewportWidth, viewportHeight] = (process.env.REALM_WEB_VIEWPORT || '1280x820')
  .split('x')
  .map((part) => Number.parseInt(part, 10));
const deviceScaleFactor = Number.parseFloat(process.env.REALM_WEB_DEVICE_SCALE_FACTOR || '1');
const isMobile = process.env.REALM_WEB_IS_MOBILE === '1';
const browser = await chromium.launch();
const page = await browser.newPage({
  viewport: { width: viewportWidth, height: viewportHeight },
  deviceScaleFactor,
  isMobile,
  hasTouch: isMobile,
});

const errors = [];
page.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});
page.on('pageerror', (err) => errors.push(err.message));

await page.goto(url, { waitUntil: 'domcontentloaded' });
await page.waitForSelector('canvas', { timeout: 30000 });
await page.waitForFunction(() => globalThis.realmReady === true, null, { timeout: 60000 });
await page.waitForFunction(() => {
  const module = globalThis.Module;
  return module && typeof module._realm_web_tick === 'function' && module._realm_web_tick() > 0;
}, null, { timeout: 60000 });

const canvasBox = await page.locator('canvas').boundingBox();
if (!canvasBox || canvasBox.width < 300 || canvasBox.height < 200) {
  throw new Error(`canvas too small: ${JSON.stringify(canvasBox)}`);
}

const viewportFit = await page.evaluate(() => {
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

const pixelCheck = await page.evaluate(() => {
  const canvas = document.querySelector('canvas');
  return canvas.toDataURL('image/png').length;
});
if (pixelCheck < 2000) throw new Error('canvas export is unexpectedly small');

if (errors.some((line) => /uncaught|exception|abort|content security|webassembly|wasm/i.test(line))) {
  throw new Error(`fatal browser console errors:\n${errors.join('\n')}`);
}

await browser.close();
console.log(`Realm web smoke passed at ${url}`);
