// probe light renders and numbers from probe_gi.html: node shoot_gi.mjs <out dir> "<query>"
// (the comparison image as gi.png, a sun turn's frames as gi_NNN.png with gif=N; the sweep's numbers on the console)
import { chromium } from 'playwright-core';
import http from 'http'; import fs from 'fs'; import path from 'path';
const root = process.cwd(); const port = 8970 + Math.floor(Math.random() * 100);
const server = http.createServer((q, r) => {
  const f = path.join(root, decodeURIComponent(q.url.split('?')[0]));
  const types = { '.js': 'text/javascript', '.html': 'text/html', '.gltf': 'model/gltf+json', '.json': 'application/json' };
  fs.readFile(f, (e, d) => { if (e) { r.statusCode = 404; r.end(); } else { r.setHeader('Content-Type', types[path.extname(f)] || 'application/octet-stream'); r.end(d); } });
}).listen(port);
const browser = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium_headless_shell-1194/chrome-linux/headless_shell' });
const [out, query] = process.argv.slice(2);
const page = await browser.newPage({ viewport: { width: 2000, height: 400 } });
page.on('console', (m) => console.log(m.text())); page.on('pageerror', (e) => console.log('pageerror:', e.message));
const t0 = Date.now();
await page.goto(`http://localhost:${port}/probe_gi.html?${query}`);
await page.waitForFunction(() => window.done, null, { timeout: 0, polling: 2000 });
console.log(JSON.stringify(await page.evaluate(() => window.result)));
await (await page.$('#out')).screenshot({ path: `${out}/gi.png` });
const frames = await page.evaluate(() => window.gifFrames || []);
frames.forEach((d, i) => fs.writeFileSync(`${out}/gi_${String(i).padStart(3, '0')}.png`, Buffer.from(d.split(',')[1], 'base64')));
console.log(`probe light done in ${((Date.now() - t0) / 1000).toFixed(0)} s`);
await browser.close(); server.close();
