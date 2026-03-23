#include "slmanimationengine.h"

namespace Slm::Motion {

AnimationEngine::AnimationEngine(QObject *parent)
    : QObject(parent)
{
}

void AnimationEngine::startSpringFromCurrent(const QString &channel,
                                             double current,
                                             double target,
                                             double velocity,
                                             const QString &presetName)
{
    ChannelState &state = m_channels[channel];
    state.spec = MotionPresetLibrary::specFromString(presetName);
    state.springState.value = current;
    state.springState.velocity = velocity;
    state.physicsState.value = current;
    state.physicsState.velocity = velocity;
    state.target = target;
    state.springActive = true;
    state.physicsActive = false;
    emit channelUpdated(channel, state.springState.value, state.springState.velocity, true);
}

void AnimationEngine::retarget(const QString &channel, double target)
{
    auto it = m_channels.find(channel);
    if (it == m_channels.end()) {
        return;
    }
    it->target = target;
    it->springActive = true;
}

void AnimationEngine::adoptVelocity(const QString &channel, double velocity)
{
    auto it = m_channels.find(channel);
    if (it == m_channels.end()) {
        return;
    }
    it->springState.velocity = velocity;
    it->physicsState.velocity = velocity;
}

void AnimationEngine::cancelAndSettle(const QString &channel, double target)
{
    auto it = m_channels.find(channel);
    if (it == m_channels.end()) {
        return;
    }
    it->target = target;
    it->springState.value = target;
    it->springState.velocity = 0.0;
    it->physicsState = {target, 0.0};
    it->springActive = false;
    it->physicsActive = false;
    emit channelUpdated(channel, target, 0.0, false);
}

bool AnimationEngine::isActive(const QString &channel) const
{
    const auto it = m_channels.constFind(channel);
    if (it == m_channels.constEnd()) {
        return false;
    }
    return it->springActive || it->physicsActive;
}

double AnimationEngine::value(const QString &channel) const
{
    const auto it = m_channels.constFind(channel);
    if (it == m_channels.constEnd()) {
        return 0.0;
    }
    return it->springState.value;
}

QVariantList AnimationEngine::channelsSnapshot() const
{
    QVariantList out;
    out.reserve(m_channels.size());
    for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
        QVariantMap row;
        row.insert(QStringLiteral("channel"), it.key());
        row.insert(QStringLiteral("value"), it->springState.value);
        row.insert(QStringLiteral("velocity"), it->springState.velocity);
        row.insert(QStringLiteral("target"), it->target);
        row.insert(QStringLiteral("active"), it->springActive || it->physicsActive);
        row.insert(QStringLiteral("preset"), toString(it->spec.preset));
        out.push_back(row);
    }
    return out;
}

void AnimationEngine::tick(double dtSeconds)
{
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        bool active = false;
        if (it->springActive) {
            it->springState = SpringSolver::step(it->springState, it->target, it->spec.spring, dtSeconds);
            if (SpringSolver::settled(it->springState, it->target, it->spec.spring)) {
                it->springState.value = it->target;
                it->springState.velocity = 0.0;
                it->springActive = false;
            } else {
                active = true;
            }
        }
        if (it->physicsActive) {
            it->physicsState = PhysicsIntegrator::step(it->physicsState, it->spec.physics, dtSeconds);
            it->springState.value = it->physicsState.value;
            it->springState.velocity = it->physicsState.velocity;
            if (PhysicsIntegrator::stopped(it->physicsState, it->spec.physics)) {
                it->physicsActive = false;
            } else {
                active = true;
            }
        }
        emit channelUpdated(it.key(), it->springState.value, it->springState.velocity, active);
    }
}

} // namespace Slm::Motion
