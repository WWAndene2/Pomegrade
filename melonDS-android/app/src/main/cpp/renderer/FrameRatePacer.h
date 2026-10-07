#ifndef FRAMERATEPACER_H
#define FRAMERATEPACER_H

#include <atomic>

// Frame rate modes (Pomegrade): how many images the screen gets for each DS
// frame (the DS runs at 60).
//   30: one DS frame in two is shown (the game runs at its own speed; the
//       hidden frames skip the display-only work: lighting effects, frame
//       generation, presentation)
//   60: every DS frame, nothing generated
//   120, 240: 1 or 3 images generated between two DS frames
//   adaptive: the highest of these the screen shows and the phone keeps up
//       with, lowered while the phone is hot
// Images the screen can't show are not generated: presented in order, the
// extra ones would be dropped unevenly (judder). A 90 Hz screen shows 60,
// 144 Hz 120, 240 Hz 240.
// Inputs come from any thread; NextFrame and FrameDone from the emulation thread.
class FrameRatePacer
{
public:
    static constexpr int Adaptive = 0;

    struct Plan
    {
        bool Show = true;     // the DS frame is presented
        int Generated = 0;    // images generated after it (0, 1 or 3)
        bool NextShow = true; // the next DS frame is presented: the DS renders its 3D during this one
                              // (line 215, after this frame's 3D is composited at line 192)
    };

    void SetMode(int mode);             // 30, 60, 120, 240 or Adaptive
    void SetDisplayRate(float hz);      // the screen's current refresh rate
    void SetThermalLimit(bool limited); // the phone is hot: adaptive stays at 60 at most
    void SetGenerationAvailable(bool available); // images can be generated (OpenGL renderer): otherwise 60 at most

    // before each DS frame runs
    Plan NextFrame();
    // after it: the emulation thread's work for the frame (generated images
    // included), and the time since the previous frame started (both ms).
    // Stalls (a pause, shader compilation, a save state load) are ignored
    void FrameDone(double workMs, double intervalMs);

    // the rate shown now, in images per second (adaptive's current choice)
    int CurrentRate() const { return Level.load(std::memory_order_relaxed); }

    // the highest rate a screen of that refresh rate shows (60, 120 or 240)
    static int DisplayCap(float hz);

private:
    std::atomic<int> Mode{60};
    std::atomic<float> DisplayRate{60.0f};
    std::atomic<bool> Thermal{false};
    std::atomic<bool> GenerationAvailable{true};
    std::atomic<int> Level{60};

    // emulation thread
    long long FrameCount = 0;
    int AppliedMode = -1;
    double WindowWork = 0, WindowInterval = 0;
    int WindowFrames = 0;
    int CalmWindows = 0;

    int TargetFor(int mode) const;
};

#endif // FRAMERATEPACER_H
