import { chromium } from 'playwright-core';
import http from 'http'; import fs from 'fs'; import path from 'path';
const root = process.cwd(); const port = 8870 + Math.floor(Math.random() * 100);
const server = http.createServer((q, r) => {
  const f = path.join(root, decodeURIComponent(q.url.split('?')[0]));
  const types = { '.js': 'text/javascript', '.html': 'text/html', '.gltf': 'model/gltf+json', '.json': 'application/json' };
  fs.readFile(f, (e, d) => { if (e) { r.statusCode = 404; r.end(); } else { r.setHeader('Content-Type', types[path.extname(f)] || 'application/octet-stream'); r.end(d); } });
}).listen(port);
const browser = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium_headless_shell-1194/chrome-linux/headless_shell', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
page.on('pageerror', (e) => console.log('pageerror:', e.message)); page.on('console', (m) => console.log('page:', m.text()));
for (const pass of (process.argv[3] || 'base,relief,flat').split(',')) {
  await page.goto(`http://localhost:${port}/passes.html?pass=${pass}&${process.argv[2]}`);
  await page.waitForFunction(() => window.done, null, { timeout: 0, polling: 500 });
  fs.writeFileSync(`pass_${pass}.png`, Buffer.from((await page.evaluate(() => window.image)).split(',')[1], 'base64'));
  console.log(pass, 'ok');
}
await browser.close(); server.close();
