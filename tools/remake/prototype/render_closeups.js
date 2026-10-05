// scratch tool: the same spots of a town in two scenes (Platinum's reference and the ORAS rebuild), close up
// argv: gltf, prefix, offset x, offset z, then spots "name:col:row" in the town's 40-tile window
const { chromium } = require(process.env.PLAYWRIGHT || 'playwright');
const S = process.env.REMAKE_WORK || '.';
const [file, prefix, ox, oz, ...spots] = process.argv.slice(2);
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await b.newPage({ viewport: { width: 1280, height: 800 } });
  await page.goto('file://' + require('path').resolve(__dirname, '../editor/world_editor.html'));
  await page.click('#tab3d');
  await page.setInputFiles('#open3d', S + '/' + file);
  await page.waitForFunction(() => gl3d !== null, null, { timeout: 180000 });
  for (const spot of spots) {
    const [name, col, row] = spot.split(':');
    const t = [(+col - 20) * 18 + +ox, 0, (+row - 20) * 18 + +oz];
    await page.evaluate((t) => { msg3d('', false); cam.target = t; cam.dist = 230; cam.pitch = 0.75; cam.yaw = 0; draw3d(); }, t);
    await page.waitForTimeout(250);
    await page.screenshot({ path: S + '/cu_' + prefix + '_' + name + '.png', clip: { x: 0, y: 90, width: 960, height: 700 } });
  }
  await b.close();
})();
