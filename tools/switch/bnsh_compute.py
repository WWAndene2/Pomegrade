# Builds a one-variation compute BNSH by putting Maxwell code (from spirv_to_maxwell) into a copy of a
# game BNSH that already holds a compute shader (template), adjusting the NVN control fields.
# Usage: bnsh_compute.py <template.bnsh> <code.bin> <gprs> <local_x> <out.bnsh>
import struct, sys
tpl, code_path, gprs, local_x, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
d = bytearray(open(tpl, 'rb').read())
code = open(code_path, 'rb').read()
grsc = 0x60
assert d[:4] == b'BNSH' and d[grsc:grsc + 4] == b'grsc'
assert struct.unpack_from('<I', d, grsc + 0x1C)[0] == 1, 'template must have one variation'
variation = struct.unpack_from('<Q', d, grsc + 0x20)[0]
program = struct.unpack_from('<Q', d, variation + 0x10)[0]
stages = struct.unpack_from('<6Q', d, program + 8)
assert stages[:5] == (0, 0, 0, 0, 0) and stages[5], 'template must be compute only'
stage = stages[5]
control, code_off = struct.unpack_from('<Q', d, stage + 0x08)[0], struct.unpack_from('<Q', d, stage + 0x10)[0]
code_region = struct.unpack_from('<I', d, stage + 0x18)[0]
assert struct.unpack_from('<I', d, code_off)[0] == 0x12345679, 'compute code header'
assert struct.unpack_from('<I', d, control)[0] == 0x98761234, 'NVN control magic'
end = 0x80 + len(code)
assert end <= code_region, 'code does not fit the template region (%#x > %#x)' % (end, code_region)
d[code_off + 0x80:code_off + code_region] = bytes(code_region - 0x80)
d[code_off + 0x80:code_off + end] = code
# Control fields (offsets from the control start), named by comparing the game's shaders:
struct.pack_into('<I', d, control + 1784, end)  # end of instructions
struct.pack_into('<I', d, control + 1788, 0)    # embedded constant data size (nouveau inlines immediates)
struct.pack_into('<I', d, control + 1804, gprs) # registers per thread
struct.pack_into('<III', d, control + 1864, local_x, 1, 1)  # workgroup size
open(out, 'wb').write(d)
print('ok: code %#x bytes at %#x, region %#x' % (len(code), code_off + 0x80, code_region))
