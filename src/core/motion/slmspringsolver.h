#pragma once

#include "slmmotiontypes.h"

namespace Slm::Motion {

class SpringSolver
{
public:
    struct State {
        double value = 0.0;
        double velocity = 0.0;
    };

    static State step(const State &current,
                      double target,
                      const SpringParams &params,
                      double dtSeconds);

    static bool settled(const State &state,
                        double target,
                        const SpringParams &params);
};

} // namespace Slm::Motion
