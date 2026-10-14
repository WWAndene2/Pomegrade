#ifndef GPU3D_WORLDFRAME_H
#define GPU3D_WORLDFRAME_H

#include <vector>

namespace melonDS
{
// Pomegrade: a frame of reference that stays with the scene while the camera
// turns. The DS hands the renderer view-space geometry only, and games often
// set their lights in view space too (or on the character), so a light taken
// as the game gives it turns with the camera. This follows the camera's
// rotation from frame to frame by aligning the scenery's vertices with the
// previous frame's (iterative closest point: no vertex correspondence needed,
// the DS clips and culls differently as the view moves), and accumulates it.
// When the scenery cannot be aligned (a cut, a new scene, too little of it
// left in common), it starts again from the current view.
class WorldFrame
{
public:
    // The frame's scenery vertices, view space (x, y, z per vertex). Returns
    // true when the frame of reference started again (the previous view was
    // not found in this one)
    bool Update(const std::vector<float>& points);
    // a view-space direction to the scene's frame of reference, and back
    void ViewToWorld(const float in[3], float out[3]) const noexcept;
    void WorldToView(const float in[3], float out[3]) const noexcept;
    void Reset() noexcept;

private:
    std::vector<float> Previous;
    float ToWorld[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; // view -> scene, row-major
};

// The rigid motion best mapping points a onto points b (least squares, Horn's
// quaternion method): b ~ R a + t. R row-major. False if degenerate
bool FitRigidMotion(const std::vector<float>& a, const std::vector<float>& b, float r[9], float t[3]);
}

#endif
