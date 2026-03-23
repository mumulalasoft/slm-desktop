#pragma once

#include "slmmotiontypes.h"

namespace Slm::Motion {

class MotionPresetLibrary
{
public:
    static MotionPresetSpec spec(MotionPreset preset);
    static MotionPresetSpec specFromString(const QString &name);
};

} // namespace Slm::Motion
