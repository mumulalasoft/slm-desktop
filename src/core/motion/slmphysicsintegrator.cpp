#include "slmphysicsintegrator.h"

#include <QtMath>

namespace Slm::Motion {

PhysicsIntegrator::State PhysicsIntegrator::step(const State &current,
                                                 const PhysicsParams &params,
                                                 double dtSeconds)
{
    const double safeDt = qBound(0.0001, dtSeconds, 0.05);
    const double clampedVelocity = qBound(-params.maxVelocity, current.velocity, params.maxVelocity);

    const double decay = qExp(-params.friction * safeDt);

    State next;
    next.velocity = clampedVelocity * decay;
    next.value = current.value + next.velocity * safeDt;
    return next;
}

bool PhysicsIntegrator::stopped(const State &state,
                                const PhysicsParams &params)
{
    return qAbs(state.velocity) <= params.stopVelocity;
}

} // namespace Slm::Motion
