#ifndef GPU3D_SKELETONRECOVERY_H
#define GPU3D_SKELETONRECOVERY_H

// Skeleton recovery (Pomegrade, DS_ENGINE_REMAKE.md 5.10 experiment 2, 5.12
// step 10): a skinned model is drawn joint by joint with the matrix stack.
// Each joint's matrix is its parent's times its own transform, kept in a
// stack slot: MTX_RESTORE parent, MTX_MULT/TRANS/SCALE..., MTX_STORE joint
// (or chained from the joint just stored). Its polygons are drawn after
// MTX_RESTORE of its slot (and the model's MTX_SCALE); a skinned polygon
// switches slots from vertex to vertex. From one frame's command trace this
// gives each model's joint tree and the vertices each joint moves. A model's
// drawing starts with the projection matrix loaded (the SDK does it for each
// model); a stack slot may hold several joints of one model in turn.

#include "types.h"

#include <string>
#include <vector>

namespace melonDS
{

// one geometry command parameter, as the geometry engine ran it
struct GeometryCommand
{
    u8 Command;
    u32 Param;
    u32 Site; // call site of the draw (identifies the model)
};

struct Joint
{
    int Slot = 0;        // matrix stack slot it is kept in
    int Parent = -1;     // index of the parent joint in Skeleton::Joints, -1: the model's root
    u32 Vertices = 0;    // vertices drawn with its matrix
};

struct Skeleton
{
    u32 Site = 0;        // call site of its first joint
    size_t FirstCommand = 0; // index in the trace
    std::vector<Joint> Joints;
    [[nodiscard]] int Depth() const;
    [[nodiscard]] u32 Vertices() const;
};

// models with at least minJoints joints, in drawing order
std::vector<Skeleton> RecoverSkeletons(const std::vector<GeometryCommand>& trace, size_t minJoints = 3);

// a skeleton as an indented tree, one joint a line
std::string SkeletonTree(const Skeleton& skeleton);

}

#endif // GPU3D_SKELETONRECOVERY_H
