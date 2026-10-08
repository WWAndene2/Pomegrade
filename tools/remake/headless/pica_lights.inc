// Pomegrade headless: the light of each draw, while "lights on" runs (retro_host, pomegrade_report.inc). Inserted into
// pica_core.cpp by patch_core.py: the fragment light registers (LightingRegs) and the vertex shader's float uniforms, where
// the field's hemisphere light (HslSCol, HslGCol, HslSDir: Gfx_BindStandardUniforms 0x396510) goes. A reading after the
// frame shows only its last draw (the bottom screen), so each draw's state is kept: its distinct values and their counts.
#include <cstring>
#include <map>
#include <string>
bool pomegrade_lights_on = false;
unsigned pomegrade_light_draws = 0;
std::map<std::string, unsigned> pomegrade_light_states;
std::map<std::string, unsigned> pomegrade_uniform_values[96];
static void PomegradeRecordLights(const Pica::RegsInternal& r, const Pica::Uniforms& u) {
    pomegrade_light_draws++;
    const auto& l = r.lighting;
    std::string s;
    char b[256];
    snprintf(b, sizeof b, "fragment %u disable %u lights %u", (unsigned)r.texturing.fragment_lighting_enable.Value(),
             (unsigned)l.disable.Value(), (unsigned)l.max_light_index.Value() + 1);
    s += b;
    const unsigned n = r.texturing.fragment_lighting_enable ? (unsigned)l.max_light_index.Value() + 1 : 0;
    for (unsigned i = 0; i < n; i++) {
        u32 w[16];
        std::memcpy(w, &l.light[i], sizeof w);
        // words 0-3 specular 0, specular 1, diffuse, ambient (10-bit r g b, 255 = 1.0); 4-5 the position or direction
        // (16-bit floats x y, z); 9 config (bit 0 directional)
        snprintf(b, sizeof b, " | light %u spec0 %08X spec1 %08X diff %08X amb %08X pos %08X %08X cfg %08X", i, w[0], w[1],
                 w[2], w[3], w[4], w[5], w[9]);
        s += b;
    }
    pomegrade_light_states[s]++;
    for (unsigned i = 0; i < 96; i++) {
        snprintf(b, sizeof b, "%g %g %g %g", u.f[i].x.ToFloat32(), u.f[i].y.ToFloat32(), u.f[i].z.ToFloat32(),
                 u.f[i].w.ToFloat32());
        auto& m = pomegrade_uniform_values[i];
        if (m.size() < 32 || m.count(b)) m[b]++;
    }
}
