# Shared helpers for the compositing: the camera, world positions from the depth pass, projection,
# and a deterministic noise (fixed seeds: the same image every run).
import numpy as np
from PIL import Image
from scipy import ndimage

def load(p): return np.asarray(Image.open(p).convert('RGB')).astype(np.float32) / 255
def to_lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def to_srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
def smoothstep(a, b, x): t = np.clip((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t)
LUMA = np.array([0.2126, 0.7152, 0.0722], np.float32)
SUN_AZ, SUN_EL = 160.0, 32.0
SUN = np.array([np.cos(np.radians(SUN_EL)) * np.cos(np.radians(SUN_AZ)), np.sin(np.radians(SUN_EL)),
                np.cos(np.radians(SUN_EL)) * np.sin(np.radians(SUN_AZ))])

class Camera:
    def __init__(self, spec, w=960, h=540, fov=40.0):
        c = np.array([float(v) for v in spec.split(',')]); self.pos, tgt = c[:3], c[3:]
        z = self.pos - tgt; self.z = z / np.linalg.norm(z)
        x = np.cross([0, 1, 0], self.z); self.x = x / np.linalg.norm(x); self.y = np.cross(self.z, self.x)
        self.w, self.h, self.t = w, h, np.tan(np.radians(fov) / 2)
    def view(self, v): return np.array([v @ self.x, v @ self.y, v @ self.z])
    def world_positions(self, depth):
        # depth = view distance along -z (passes.html packs it); pixel rays from the field of view
        ys, xs = np.mgrid[0:self.h, 0:self.w].astype(np.float32)
        nx = (xs + 0.5) / self.w * 2 - 1; ny = 1 - (ys + 0.5) / self.h * 2
        a = self.w / self.h
        vx, vy = nx * self.t * a * depth, ny * self.t * depth
        return (self.pos + vx[..., None] * self.x + vy[..., None] * self.y - depth[..., None] * self.z)
    def project(self, P):
        d = P - self.pos; vz = -(d @ self.z); vx = d @ self.x; vy = d @ self.y
        a = self.w / self.h
        sx = (vx / (vz * self.t * a) + 1) / 2 * self.w; sy = (1 - vy / (vz * self.t)) / 2 * self.h
        return sx, sy, vz

def depth_of(folder):
    d = load(f'{folder}/pass_depth.png'); z = (d[..., 0] * 255 + d[..., 1]) / 255 * 1500
    return z

_rng_tables = {}
def value_noise(x, y, seed=1):
    # smooth 2D value noise on a lattice, deterministic
    if seed not in _rng_tables: _rng_tables[seed] = np.random.default_rng(seed).random((256, 256)).astype(np.float32)
    T = _rng_tables[seed]
    xi, yi = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = x - xi, y - yi
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    def at(i, j): return T[j % 256, i % 256]
    return (at(xi, yi) * (1 - fx) + at(xi + 1, yi) * fx) * (1 - fy) + (at(xi, yi + 1) * (1 - fx) + at(xi + 1, yi + 1) * fx) * fy

def fbm(x, y, octaves=5, seed=1):
    s, a, f, norm = 0, 1.0, 1.0, 0
    for o in range(octaves):
        s = s + a * value_noise(x * f, y * f, seed + o); norm += a; a *= 0.5; f *= 2.03
    return s / norm
