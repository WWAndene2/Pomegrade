#include "GPU3D_LightRecovery.h"

namespace melonDS
{

std::vector<LightSetting> RecoverLights(const std::vector<GeometryCommand>& trace)
{
    std::vector<LightSetting> out;
    LightSetting current[4];
    for (int i = 0; i < 4; i++) current[i].Light = i;
    // a light's setting is complete when both its direction and colour are
    // known; each command that changes either records the setting it makes
    auto record = [&](int light, u32 site) {
        LightSetting& c = current[light];
        if (!c.HasDirection || !c.HasColour) return;
        for (LightSetting& s : out)
            if (s.Light == light && s.Colour == c.Colour && s.Direction[0] == c.Direction[0] &&
                s.Direction[1] == c.Direction[1] && s.Direction[2] == c.Direction[2])
            {
                s.Times++;
                return;
            }
        LightSetting s = c;
        s.Times = 1;
        s.FirstSite = site;
        out.push_back(s);
    };
    auto component = [](u32 v) { return (float)((s32)(v << 22) >> 22) / 512.0f; };
    for (const GeometryCommand& c : trace)
    {
        const int light = (c.Param >> 30) & 3;
        if (c.Command == 0x32) // LIGHT_VECTOR
        {
            current[light].Direction[0] = component(c.Param & 0x3FF);
            current[light].Direction[1] = component((c.Param >> 10) & 0x3FF);
            current[light].Direction[2] = component((c.Param >> 20) & 0x3FF);
            current[light].HasDirection = true;
            record(light, c.Site);
        }
        else if (c.Command == 0x33) // LIGHT_COLOR
        {
            current[light].Colour = c.Param & 0x7FFF;
            current[light].HasColour = true;
            record(light, c.Site);
        }
    }
    return out;
}

}
