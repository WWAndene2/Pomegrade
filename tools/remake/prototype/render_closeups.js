// The same spots of a town in two scenes (Platinum's reference and the ORAS rebuild), close up, from the game's camera angle
// or, with --top first, from straight above (zone shapes and edges read best there)
// argv: [--top] gltf, prefix, offset x, offset z, then spots "name:col:row" in the town's 40-tile window;
// writes cu_<prefix>_<name>.png in REMAKE_WORK
const { chromium } = require(process.env.PLAYWRIGHT || 'playwright');
const S = process.env.REMAKE_WORK || '.';
const args = process.argv.slice(2);
const top = args[0] === '--top';
const [file, prefix, ox, oz, ...spots] = top ? args.slice(1) : args;
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
    const view = top ? { dist: 160, pitch: 1.45 } : { dist: 230, pitch: 0.75 };
    await page.evaluate(([t, v]) => { msg3d('', false); cam.target = t; cam.dist = v.dist; cam.pitch = v.pitch; cam.yaw = 0; draw3d(); }, [t, view]);
    await page.waitForTimeout(250);
    await page.screenshot({ path: S + '/cu_' + prefix + '_' + name + '.png', clip: { x: 0, y: 90, width: 960, height: 700 } });
  }
  await b.close();
})();
