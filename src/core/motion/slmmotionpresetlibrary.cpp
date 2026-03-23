#include "slmmotionpresetlibrary.h"

namespace Slm::Motion {

MotionPresetSpec MotionPresetLibrary::spec(MotionPreset preset)
{
    MotionPresetSpec out;
    out.preset = preset;
    switch (preset) {
    case MotionPreset::Snappy:
        out.spring = {740.0, 56.0, 1.0, 0.001};
        out.time = {130.0, 0.0};
        out.physics = {12.0, 8.0, 6500.0};
        break;
    case MotionPreset::Smooth:
        out.spring = {520.0, 46.0, 1.0, 0.001};
        out.time = {180.0, 0.0};
        out.physics = {10.5, 6.0, 6000.0};
        break;
    case MotionPreset::Gentle:
        out.spring = {360.0, 38.0, 1.0, 0.001};
        out.time = {240.0, 0.0};
        out.physics = {8.0, 5.0, 5000.0};
        break;
    case MotionPreset::Responsive:
        out.spring = {620.0, 50.0, 1.0, 0.001};
        out.time = {160.0, 0.0};
        out.physics = {11.0, 7.0, 6200.0};
        break;
    case MotionPreset::Launcher:
        // Slightly underdamped launchpad reveal: quick with a subtle premium overshoot.
        out.spring = {700.0, 42.0, 1.0, 0.001};
        out.time = {170.0, 0.0};
        out.physics = {10.0, 7.0, 6200.0};
        break;
    case MotionPreset::Heavy:
        out.spring = {300.0, 44.0, 1.45, 0.001};
        out.time = {260.0, 0.0};
        out.physics = {14.0, 10.0, 4200.0};
        break;
    case MotionPreset::Elastic:
        out.spring = {760.0, 28.0, 1.0, 0.001};
        out.time = {220.0, 0.0};
        out.physics = {9.0, 7.0, 6500.0};
        break;
    }
    return out;
}

MotionPresetSpec MotionPresetLibrary::specFromString(const QString &name)
{
    return spec(motionPresetFromString(name));
}

} // namespace Slm::Motion
