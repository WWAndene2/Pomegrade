// capture.js <scene dir> <view>...: serves render/ and the scene dir (data.js, fine.js, tex/, models/), renders
// scene.html?v=<view> in headless Chromium (SwiftShader) and saves <scene>/view<view>.png from the canvas (a page
// screenshot times out on scenes this heavy). Views: 0 overview, 1 valley, 2 peaks, 3 ground level.
const {chromium}=require('playwright'); const fs=require('fs'), http=require('http'), path=require('path');
const scene=path.resolve(process.argv[2]), render=__dirname;
const types={'.html':'text/html','.js':'text/javascript','.png':'image/png','.json':'application/json'};
const server=http.createServer((q,r)=>{const rel=decodeURIComponent(q.url.split('?')[0]).replace(/^\/+/,'');
  for(const root of [scene,render]){const f=path.join(root,rel); if(f.startsWith(root)&&fs.existsSync(f)&&fs.statSync(f).isFile()){
    r.writeHead(200,{'Content-Type':types[path.extname(f)]||'application/octet-stream'}); fs.createReadStream(f).pipe(r); return;}}
  r.writeHead(404); r.end();}).listen(0,'127.0.0.1',async()=>{
  const port=server.address().port;
  const b=await chromium.launch({args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
  const p=await b.newPage({viewport:{width:1600,height:900}}); p.setDefaultTimeout(1500000);
  p.on('pageerror',e=>{console.log('page error',e.message);process.exit(1);});
  for(const v of process.argv.slice(3)){
    await p.goto(`http://127.0.0.1:${port}/scene.html?v=${v}`);
    await p.waitForFunction(()=>document.title==='done',null,{timeout:1500000});
    const d=await p.evaluate(()=>document.querySelector('canvas').toDataURL('image/png'));
    fs.writeFileSync(path.join(scene,`view${v}.png`),Buffer.from(d.split(',')[1],'base64')); console.log('view',v);
  }
  await b.close(); server.close();});
