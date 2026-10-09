// ray-traced ambient occlusion frames from ao.html: node shoot_ao.mjs <out dir> "<query>"
import { chromium } from 'playwright-core';
import http from 'http'; import fs from 'fs'; import path from 'path';
const root = process.cwd(); const port = 8970 + Math.floor(Math.random() * 100);
const server = http.createServer((q, r) => {
  const f = path.join(root, decodeURIComponent(q.url.split('?')[0]));
  const types = { '.js': 'text/javascript', '.html': 'text/html', '.gltf': 'model/gltf+json', '.json': 'application/json' };
  fs.readFile(f, (e, d) => { if (e) { r.statusCode = 404; r.end(); } else { r.setHeader('Content-Type', types[path.extname(f)] || 'application/octet-stream'); r.end(d); } });
}).listen(port);
const browser = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium_headless_shell-1194/chrome-linux/headless_shell', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
const [out, query] = process.argv.slice(2);
const page = await browser.newPage();
page.on('console', (m) => console.log(m.text())); page.on('pageerror', (e) => console.log('pageerror:', e.message));
const t0 = Date.now();
await page.goto(`http://localhost:${port}/ao.html?${query}`);
await page.waitForFunction(() => window.done, null, { timeout: 0, polling: 1000 });
const imgs = await page.evaluate(() => window.images);
imgs.forEach((d, i) => fs.writeFileSync(`${out}/ao_${i}.png`, Buffer.from(d.split(',')[1], 'base64')));
console.log(`ao ${imgs.length} frames in ${((Date.now() - t0) / 1000).toFixed(0)} s`);
await browser.close(); server.close();
