#pragma once

#include "slmmotiontypes.h"

namespace Slm::Motion {

class PhysicsIntegrator
{
public:
    struct State {
        double value = 0.0;
        double velocity = 0.0;
    };

    static State step(const State &current,
                      const PhysicsParams &params,
                      double dtSeconds);

    static bool stopped(const State &state,
                        const PhysicsParams &params);
};

} // namespace Slm::Motion
