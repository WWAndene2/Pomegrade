// SPIR-V compute shader -> NIR -> Mesa nouveau codegen for Maxwell (GM20x, the Switch GPU) -> raw instructions.
// Usage: spirv_to_maxwell <in.spv> <out.bin>; build steps in tools/switch/README.md.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "compiler/spirv/nir_spirv.h"
#include "compiler/nir/nir.h"
#include "codegen/nv50_ir_driver.h"
#include "util/u_debug.h"
#include "compiler/nir/nir_builder.h"
// Vulkan descriptors -> plain buffer indices: UBO binding b becomes constant buffer index b (offset 0).
static bool lower_vk_res(nir_builder *b, nir_intrinsic_instr *in, void *) {
  if (in->intrinsic == nir_intrinsic_vulkan_resource_index) {
    b->cursor = nir_before_instr(&in->instr);
    nir_def *idx = nir_iadd_imm(b, in->src[0].ssa, nir_intrinsic_binding(in));
    nir_def_replace(&in->def, nir_vec2(b, idx, nir_imm_int(b, 0)));
    return true;
  }
  if (in->intrinsic == nir_intrinsic_load_vulkan_descriptor) {
    nir_def_replace(&in->def, in->src[0].ssa);
    return true;
  }
  return false;
}
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  std::vector<uint32_t> w(n / 4); fread(w.data(), 4, w.size(), f); fclose(f);
  glsl_type_singleton_init_or_ref();
  const int chip = 0x120;  // GM20B (Tegra X1) is a GM200-class Maxwell
  const nir_shader_compiler_options *no = nv50_ir_nir_shader_compiler_options(chip, MESA_SHADER_COMPUTE);
  spirv_to_nir_options so = {};
  so.environment = NIR_SPIRV_VULKAN;
  so.ubo_addr_format = nir_address_format_32bit_index_offset;
  so.ssbo_addr_format = nir_address_format_32bit_index_offset;
  so.phys_ssbo_addr_format = nir_address_format_64bit_global;
  so.push_const_addr_format = nir_address_format_32bit_offset;
  so.shared_addr_format = nir_address_format_32bit_offset;
  nir_shader *s = spirv_to_nir(w.data(), w.size(), NULL, MESA_SHADER_COMPUTE, "main", &so, no);
  if (!s) { fprintf(stderr, "spirv_to_nir failed\n"); return 1; }
  NIR_PASS(_, s, nir_lower_variable_initializers, nir_var_function_temp);
  NIR_PASS(_, s, nir_lower_returns);
  NIR_PASS(_, s, nir_inline_functions);
  NIR_PASS(_, s, nir_opt_deref);
  nir_remove_non_entrypoints(s);
  NIR_PASS(_, s, nir_lower_vars_to_ssa);
  NIR_PASS(_, s, nir_shader_intrinsics_pass, lower_vk_res, nir_metadata_control_flow, NULL);
  NIR_PASS(_, s, nir_lower_explicit_io, nir_var_mem_ubo, nir_address_format_32bit_index_offset);
  NIR_PASS(_, s, nir_lower_explicit_io, nir_var_mem_global, nir_address_format_64bit_global);
  NIR_PASS(_, s, nir_lower_system_values);
  nir_lower_compute_system_values_options cso = {}; cso.global_id_is_32bit = true;
  NIR_PASS(_, s, nir_lower_compute_system_values, &cso);
  NIR_PASS(_, s, nir_opt_dce);
  if (getenv("PRINT_NIR")) nir_print_shader(s, stderr);
  nv50_ir_prog_info info = {};
  info.type = MESA_SHADER_COMPUTE; info.target = chip; info.optLevel = 3;
  info.dbgFlags = getenv("DBG") ? atoi(getenv("DBG")) : 0;
  info.bin.nir = s;
  info.io.auxCBSlot = 7; info.io.msInfoCBSlot = 7;
  info.io.uboInfoBase = 0x200; info.io.bufInfoBase = 0x100; info.io.suInfoBase = 0x300;
  info.io.texBindBase = 0x400; info.io.bindlessBase = 0x500;
  info.prop.cp.gridInfoBase = 0x10;
  nv50_ir_prog_info_out out = {};
  int r = nv50_ir_generate_code(&info, &out);
  if (r) { fprintf(stderr, "codegen failed %d\n", r); return 1; }
  f = fopen(argv[2], "wb"); fwrite(out.bin.code, 1, out.bin.codeSize, f); fclose(f);
  printf("code %u bytes, gprs %u, tls %u, smem %u\n", out.bin.codeSize, out.bin.maxGPR + 1, out.bin.tlsSpace, out.bin.smemSize);
  return 0;
}
// Compute shaders have no varyings: stubs for the two TGSI helpers the NIR converter references.
extern "C" void tgsi_get_gl_varying_semantic(int, bool, unsigned *n, unsigned *i) { *n = 0; *i = 0; abort(); }
extern "C" void tgsi_get_gl_frag_result_semantic(int, unsigned *n, unsigned *i) { *n = 0; *i = 0; abort(); }
