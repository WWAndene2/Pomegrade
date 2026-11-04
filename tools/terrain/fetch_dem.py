# Fetches the real elevation data the synthesis learns from (patterns only: no place is copied 1:1).
#   fetch_dem.py copernicus <out dir> [tile ...]   Copernicus DEM GLO-30 (30 m) tiles from AWS's open data, e.g.
#                                                  N46_00_E007_00 (Valais), N46_00_E008_00 (Bernese Oberland),
#                                                  N46_00_E011_00 (Dolomites); credit: "produced using Copernicus
#                                                  WorldDEM-30 (c) DLR e.V. 2010-2014 and (c) Airbus Defence and Space
#                                                  GmbH 2014-2018, provided under COPERNICUS by the European Union and ESA"
#   fetch_dem.py swiss <out dir> <lon0> <lat0> <lon1> <lat1>
#                                                  swissALTI3D (2 m, swisstopo open data) tiles of that box, the most
#                                                  recent survey of each 1 km tile, joined into <out>/mosaic.npy
#                                                  (default box: Grindelwald, 10 x 10 km)
import sys, os, json, re, urllib.request
from concurrent.futures import ThreadPoolExecutor
COP = 'https://copernicus-dem-30m.s3.amazonaws.com/Copernicus_DSM_COG_10_{t}_DEM/Copernicus_DSM_COG_10_{t}_DEM.tif'
STAC = 'https://data.geo.admin.ch/api/stac/v0.9/collections/ch.swisstopo.swissalti3d/items?bbox={},{},{},{}&limit=100'

def copernicus(out, tiles):
    os.makedirs(out, exist_ok=True)
    for t in tiles or ['N46_00_E007_00', 'N46_00_E008_00', 'N46_00_E011_00']:
        path = os.path.join(out, t + '.tif')
        if not os.path.exists(path): urllib.request.urlretrieve(COP.format(t=t), path)
        print(t, os.path.getsize(path))

def swiss(out, box):
    import numpy as np, tifffile
    os.makedirs(out, exist_ok=True)
    best, url = {}, STAC.format(*box)
    while url:
        d = json.load(urllib.request.urlopen(url))
        for f in d['features']:
            y, e, n = map(int, re.match(r'swissalti3d_(\d+)_(\d+)-(\d+)', f['id']).groups())
            href = [v['href'] for k, v in f['assets'].items() if k.endswith('_2_2056_5728.tif')]
            if href and ((e, n) not in best or best[(e, n)][0] < y): best[(e, n)] = (y, href[0])
        url = next((l['href'] for l in d.get('links', []) if l['rel'] == 'next'), None)
    def get(k):
        path = os.path.join(out, '%d-%d.tif' % k)
        if not os.path.exists(path): urllib.request.urlretrieve(best[k][1], path)
        return k, path
    with ThreadPoolExecutor(8) as ex: paths = dict(ex.map(get, best))
    es = sorted({k[0] for k in best}); ns = sorted({k[1] for k in best})
    m = np.full((len(ns) * 500, len(es) * 500), np.nan, np.float32)
    for (e, n), p in paths.items():
        r, c = (ns[-1] - n) * 500, (e - es[0]) * 500; m[r:r + 500, c:c + 500] = tifffile.imread(p)
    np.save(os.path.join(out, 'mosaic.npy'), m)
    print(len(best), 'tiles,', m.shape, 'missing', int(np.isnan(m).sum()))

if __name__ == '__main__':
    if sys.argv[1] == 'copernicus': copernicus(sys.argv[2], sys.argv[3:])
    elif sys.argv[1] == 'swiss': swiss(sys.argv[2], [float(v) for v in sys.argv[3:7]] if len(sys.argv) >= 7 else [7.96, 46.55, 8.07, 46.63])
