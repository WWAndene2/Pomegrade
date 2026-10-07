// One model seen from four sides, framed on its bounds: the previews shown to the owner before an asset customization is
// built into a mod (DEVELOPMENT_NOTE.md 3c). Uses the editor's 3D view, as render_closeups.js does.
// argv: gltf, prefix; writes <prefix>_front.png, _side.png, _back.png, _three_quarter.png in REMAKE_WORK
const { chromium } = require(process.env.PLAYWRIGHT || 'playwright');
const S = process.env.REMAKE_WORK || '.';
const [file, prefix] = process.argv.slice(2);
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await b.newPage({ viewport: { width: 1280, height: 800 } });
  await page.goto('file://' + require('path').resolve(__dirname, '../editor/world_editor.html'));
  await page.click('#tab3d');
  await page.setInputFiles('#open3d', S + '/' + file);
  await page.waitForFunction(() => gl3d !== null, null, { timeout: 180000 });
  // the camera on the model's centre, far enough for its largest side to fill most of the view
  const views = [['front', 0], ['three_quarter', Math.PI / 4], ['side', Math.PI / 2], ['back', Math.PI]];
  for (const [name, yaw] of views) {
    await page.evaluate((yaw) => {
      msg3d('', false);
      const { lo, hi } = gl3d;
      cam.target = [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2];
      cam.dist = Math.max(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]) * 1.6;
      cam.pitch = 0.15; cam.yaw = yaw;
      draw3d();
    }, yaw);
    await page.waitForTimeout(250);
    await page.screenshot({ path: S + '/' + prefix + '_' + name + '.png', clip: { x: 0, y: 90, width: 960, height: 700 } });
  }
  await b.close();
})();
