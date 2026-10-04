#include "GPU3D_SkeletonRecovery.h"

#include <algorithm>
#include <cstdio>
#include <functional>

namespace melonDS
{

int Skeleton::Depth() const
{
    int deepest = 0;
    for (size_t i = 0; i < Joints.size(); i++)
    {
        int d = 1;
        for (int p = Joints[i].Parent; p >= 0; p = Joints[p].Parent) d++;
        deepest = std::max(deepest, d);
    }
    return deepest;
}

u32 Skeleton::Vertices() const
{
    u32 n = 0;
    for (const Joint& j : Joints) n += j.Vertices;
    return n;
}

std::vector<Skeleton> RecoverSkeletons(const std::vector<GeometryCommand>& trace, size_t minJoints)
{
    std::vector<Skeleton> out;
    Skeleton model;
    int slotJoint[32];       // joint index (in model) last stored in each slot, -1: none
    std::fill(std::begin(slotJoint), std::end(slotJoint), -1);
    int base = -1;           // joint whose matrix is current (-1: none, the model's root)
    bool changed = false;    // the current matrix was transformed since base
    int mode = 0;            // MTX_MODE: 0 projection, 1 position, 2 position & vector, 3 texture
    bool vtx16Second = false; // the next VTX_16 parameter is a vertex's second

    auto finish = [&]() {
        if (model.Joints.size() >= minJoints) out.push_back(model);
        model = Skeleton();
        std::fill(std::begin(slotJoint), std::end(slotJoint), -1);
    };

    for (size_t i = 0; i < trace.size(); i++)
    {
        const GeometryCommand& c = trace[i];
        if (c.Command != 0x23) vtx16Second = false;
        // a command's later parameters repeat its code: act on the first only
        // (vertex data counts each vertex command below)
        const bool first = i == 0 || trace[i - 1].Command != c.Command || c.Command == 0x14 || c.Command == 0x13;
        switch (c.Command)
        {
        case 0x10: mode = c.Param & 3; break;
        case 0x13: // MTX_STORE
        {
            if (mode == 0 || mode == 3) break;
            const int slot = c.Param & 0x1F;
            if (!changed && base >= 0)
            {
                // the current joint's matrix copied to another slot: an alias
                slotJoint[slot] = base;
                break;
            }
            if (!changed) break;
            // a slot may be stored again within a model (games use one as
            // scratch): the slot now holds the new joint
            if (model.Joints.empty()) { model.Site = c.Site; model.FirstCommand = i; }
            Joint j;
            j.Slot = slot;
            j.Parent = base;
            model.Joints.push_back(j);
            slotJoint[slot] = (int)model.Joints.size() - 1;
            base = slotJoint[slot];
            changed = false;
            break;
        }
        case 0x14: // MTX_RESTORE
            if (mode == 0 || mode == 3) break;
            base = slotJoint[c.Param & 0x1F];
            changed = false;
            break;
        case 0x15: case 0x16: case 0x17: // IDENTITY, LOAD
            if (!first || mode == 3) break;
            if (mode == 0)
            {
                // the projection loaded: the drawing of a new model starts
                // (the SDK sets it for each model; saving and restoring it
                // around a billboard joint is MTX_STORE/RESTORE, not a load)
                if (c.Command != 0x15) { finish(); base = -1; changed = false; }
                break;
            }
            // a matrix the CPU computed, e.g. a billboard joint from the joint
            // restored last: it stays that joint's child
            changed = true;
            break;
        case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: // MULT, SCALE, TRANS
            if (mode == 0 || mode == 3 || !first) break;
            changed = true;
            break;
        case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: // vertices
            // VTX_16 takes two parameters a vertex, the others one
            if (c.Command == 0x23)
            {
                vtx16Second = !vtx16Second;
                if (!vtx16Second) break;
            }
            // a joint's vertices are drawn with its matrix times the model's
            // scale (MTX_RESTORE joint, MTX_SCALE): the joint restored last
            if (base >= 0) model.Joints[base].Vertices++;
            break;
        default: break;
        }
    }
    finish();
    return out;
}

std::string SkeletonTree(const Skeleton& skeleton)
{
    std::string out;
    std::function<void(int, int)> walk = [&](int parent, int depth) {
        for (size_t i = 0; i < skeleton.Joints.size(); i++)
        {
            const Joint& j = skeleton.Joints[i];
            if (j.Parent != parent) continue;
            char line[96];
            snprintf(line, sizeof(line), "%*sjoint %zu (slot %d): %u vertices\n", 4 + depth * 2, "", i, j.Slot, j.Vertices);
            out += line;
            walk((int)i, depth + 1);
        }
    };
    walk(-1, 0);
    return out;
}

}
