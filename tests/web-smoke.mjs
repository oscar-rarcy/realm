import { chromium } from 'playwright';

const url = process.env.REALM_WEB_URL || 'http://127.0.0.1:4173/';
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 820 } });

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
