# textures.py <ground layers dir> <scene dir>: copies the Scarlet ground layers the scene uses (exported by
# tools/switch/scarlet_assets/ground_layers.py) under the names scene.html loads, into <scene>/tex/.
import sys, os, shutil
LAYERS = {'grass': 0, 'grass2': 1, 'meadow': 5, 'forest': 12, 'sand': 16, 'dirt': 20, 'snow': 29,
          'rockb': 40, 'rock': 42}   # indices into global_color_layer_array, chosen by eye on its contact sheet
src, scene = sys.argv[1], sys.argv[2]
os.makedirs(os.path.join(scene, 'tex'), exist_ok=True)
for name, i in LAYERS.items():
    shutil.copy(os.path.join(src, f'{i}.png'), os.path.join(scene, 'tex', f'{name}.png'))
    shutil.copy(os.path.join(src, f'{i}_n.png'), os.path.join(scene, 'tex', f'{name}_n.png'))
print('copied', len(LAYERS), 'layers')
