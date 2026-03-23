#include "slmspringsolver.h"

#include <QtMath>

namespace Slm::Motion {

SpringSolver::State SpringSolver::step(const State &current,
                                       double target,
                                       const SpringParams &params,
                                       double dtSeconds)
{
    const double safeDt = qBound(0.0001, dtSeconds, 0.05);
    const double displacement = current.value - target;
    const double accel = (-params.stiffness * displacement - params.damping * current.velocity)
                         / qMax(0.0001, params.mass);

    State next;
    next.velocity = current.velocity + accel * safeDt;
    next.value = current.value + next.velocity * safeDt;
    return next;
}

bool SpringSolver::settled(const State &state,
                           double target,
                           const SpringParams &params)
{
    return qAbs(state.value - target) <= params.settleEpsilon
           && qAbs(state.velocity) <= params.settleEpsilon * 10.0;
}

} // namespace Slm::Motion
