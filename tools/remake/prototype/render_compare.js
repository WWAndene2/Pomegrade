const { chromium } = require(process.env.PLAYWRIGHT || 'playwright');
const S = process.env.REMAKE_WORK || '.';
// argv: gltf, output prefix, offset x, offset z
const [file, prefix, ox, oz] = process.argv.slice(2);
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await b.newPage({ viewport: { width: 1280, height: 800 } });
  await page.goto('file://' + require('path').resolve(__dirname, '../editor/world_editor.html'));
  await page.click('#tab3d');
  await page.setInputFiles('#open3d', S + '/' + file);
  await page.waitForFunction(() => gl3d !== null, null, { timeout: 180000 });
  const views = [['top', [0, 0, 0], 950, 1.35, 0], ['town', [0, 0, 70], 560, 0.6, 0], ['north', [0, 0, -120], 430, 0.55, 0], ['pond', [0, 0, 330], 380, 0.6, 0]];
  for (const [n, t, d, p, y] of views) {
    await page.evaluate(([t, d, p, y]) => { msg3d('', false); cam.target = t; cam.dist = d; cam.pitch = p; cam.yaw = y; draw3d(); }, [[t[0] + +ox, t[1], t[2] + +oz], d, p, y]);
    await page.waitForTimeout(300);
    await page.screenshot({ path: S + '/' + prefix + '_' + n + '.png', clip: { x: 0, y: 90, width: 960, height: 700 } });
  }
  await b.close();
})();
