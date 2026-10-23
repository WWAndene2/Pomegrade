#!/usr/bin/env python3
"""A CGFX file's objects, cameras and animations listed, to compare two files field by field (23 October: X/Y's intro
camera a/0/3/4 513 against Omega Ruby's a/0/3/2 1016 and 874, parsed by hand several times). Layout from SPICA
(external/SPICA/SPICA/Formats/CtrGfx), checked on those files: pointers are self-relative; DATA holds 16 (count, DICT)
pairs (5 cameras, 12 camera animations, ...); a DICT is "DICT", size, count, a root node, then 16-byte nodes {ref bit,
left, right, name, data}.

  cgfx.py <file.cgfx>...            each object; for a camera (CCAM) its view (Aim with target and twist, LookAt with
                                    target and up vector, Rotate) and projection; for an animation its frames, loop and
                                    elements: name, primitive type, flags (a Transform's bit 6+i constant and 16+i
                                    absent for curve i = scale XYZ, rotation XYZ, -, translation XYZ; a vector's bits
                                    0-2 constant, 3-5 absent), each curve slot (absent, constant value or curve offset)
"""
import struct
import sys

SECTIONS = ["models", "textures", "luts", "materials", "shaders", "cameras", "lights", "fogs", "environments",
            "skeletal anims", "texture anims", "visibility anims", "camera anims", "light anims", "fog anims", "emitters"]
PRIMITIVES = ["Float", "Integer", "Boolean", "Vector2D", "Vector3D", "Transform", "RGBA", "Texture", "QuatTransform",
              "MtxTransform"]
SLOTS = {"Transform": ["scale x", "scale y", "scale z", "rot x", "rot y", "rot z", "-", "trans x", "trans y", "trans z"],
         "Vector3D": ["x", "y", "z"], "Vector2D": ["x", "y"], "Float": ["value"]}


def u32(d, o):
    return struct.unpack_from("<I", d, o)[0]


def f32(d, o):
    return struct.unpack_from("<f", d, o)[0]


def ptr(d, o):
    v = u32(d, o)
    return o + v if v else 0


def text(d, o):
    return d[o:d.index(b"\0", o)].decode("latin1") if o else None


def dictionary(d, o):
    if d[o:o + 4] != b"DICT":
        raise ValueError(f"no DICT at 0x{o:X}")
    return [(text(d, ptr(d, o + 0x1C + 16 * i + 8)), ptr(d, o + 0x1C + 16 * i + 12)) for i in range(u32(d, o + 8))]


def camera(d, o):
    view_type, projection = u32(d, o + 0xB4), u32(d, o + 0xB8)
    v, p = ptr(d, o + 0xBC), ptr(d, o + 0xC0)
    kind = {0x80000000: "Aim", 0x40000000: "LookAt", 0x20000000: "Rotate"}.get(u32(d, v), f"0x{u32(d, v):08X}")
    floats = [f32(d, v + 8 + 4 * k) for k in range(6 if kind == "LookAt" else 4)]
    view = (f"target {floats[0]:.4g} {floats[1]:.4g} {floats[2]:.4g}, " +
            (f"up {floats[3]:.4g} {floats[4]:.4g} {floats[5]:.4g}" if kind == "LookAt" else f"twist {floats[3]:.4g}"))
    near, far = f32(d, p + 4), f32(d, p + 8)
    print(f"    camera: view type {view_type} ({kind}, flags {u32(d, v + 4)}): {view}; position "
          f"{f32(d, o + 0x48):.4g} {f32(d, o + 0x4C):.4g} {f32(d, o + 0x50):.4g}; projection {projection}, near {near:.4g} far {far:.4g}")


def animation(d, o):
    print(f"    animation: target group {text(d, ptr(d, o + 0xC))!r}, loop {u32(d, o + 0x10)}, {f32(d, o + 0x14):.4g} frames")
    for name, e in dictionary(d, ptr(d, o + 0x1C)):
        flags, kind = u32(d, e), u32(d, e + 8)
        kind = PRIMITIVES[kind] if kind < len(PRIMITIVES) else str(kind)
        slots = SLOTS.get(kind, [])
        out = []
        for i, s in enumerate(slots):
            constant_bit, absent_bit = (6 + i, 16 + i) if kind == "Transform" else (i, len(slots) + i)
            at = e + 12 + 4 * i
            if flags >> absent_bit & 1:
                out.append(f"{s} absent")
            elif flags >> constant_bit & 1:
                out.append(f"{s} = {f32(d, at):.4g}")
            else:
                out.append(f"{s} curve 0x{ptr(d, at):X}")
        print(f"      {name}: {kind}, flags 0x{flags:08X}" + (": " + ", ".join(out) if out else ""))


def main():
    for path in sys.argv[1:]:
        d = open(path, "rb").read()
        if d[:4] != b"CGFX":
            print(f"{path}: not CGFX")
            continue
        print(f"{path}: {len(d)} bytes, version 0x{u32(d, 8):08X}")
        for k in range(16):
            if u32(d, 0x1C + 8 * k):
                for name, o in dictionary(d, ptr(d, 0x1C + 8 * k + 4)):
                    tag = d[o + 4:o + 8] if d[o + 4:o + 8].isalpha() else d[o:o + 4]  # an object's word 0 is its type flags, an animation's its tag
                    print(f"  {SECTIONS[k]}: {name!r} at 0x{o:X} ({tag.decode('latin1')})")
                    if k == 5:
                        camera(d, o)
                    elif k == 12:
                        animation(d, o)


if __name__ == "__main__":
    main()
