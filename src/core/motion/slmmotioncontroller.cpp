#include "slmmotioncontroller.h"

namespace Slm::Motion {

MotionController::MotionController(QObject *parent)
    : QObject(parent)
{
    m_scheduler.setEngine(&m_engine);
    m_scheduler.start();
    connect(&m_scheduler, &AnimationScheduler::timeScaleChanged, this, &MotionController::timeScaleChanged);
    connect(&m_scheduler,
            &AnimationScheduler::droppedFrameCountChanged,
            this,
            &MotionController::droppedFrameCountChanged);
    connect(&m_scheduler, &AnimationScheduler::frameStepped, this, [this](double dt) {
        if (qFuzzyCompare(1.0 + m_lastFrameDt, 1.0 + dt)) {
            return;
        }
        m_lastFrameDt = dt;
        emit lastFrameDtChanged();
    });

    connect(&m_engine, &AnimationEngine::channelUpdated, this, [this](const QString &channel,
                                                                       double value,
                                                                       double velocity,
                                                                       bool active) {
        if (channel != m_channel) {
            return;
        }
        m_value.setValue(value);
        m_value.setVelocity(velocity);
        m_value.setActive(active);
        emit valueChanged();
        emit velocityChanged();
    });
}

double MotionController::value() const
{
    return m_value.value();
}

double MotionController::velocity() const
{
    return m_value.velocity();
}

double MotionController::lastFrameDt() const
{
    return m_lastFrameDt;
}

double MotionController::timeScale() const
{
    return m_scheduler.timeScale();
}

qulonglong MotionController::droppedFrameCount() const
{
    return m_scheduler.droppedFrameCount();
}

bool MotionController::reducedMotion() const
{
    return m_reducedMotion;
}

QString MotionController::channel() const
{
    return m_channel;
}

QString MotionController::preset() const
{
    return m_preset;
}

void MotionController::setChannel(const QString &channel)
{
    if (channel == m_channel || channel.trimmed().isEmpty()) {
        return;
    }
    m_channel = channel;
    emit channelChanged();
}

void MotionController::setPreset(const QString &preset)
{
    if (preset == m_preset || preset.trimmed().isEmpty()) {
        return;
    }
    m_preset = preset;
    emit presetChanged();
}

void MotionController::setTimeScale(double scale)
{
    m_scheduler.setTimeScale(scale);
}

void MotionController::setReducedMotion(bool enabled)
{
    if (m_reducedMotion == enabled) {
        return;
    }
    m_reducedMotion = enabled;
    emit reducedMotionChanged();
}

void MotionController::ensureRunning()
{
    if (!m_scheduler.running()) {
        m_scheduler.start();
    }
}

void MotionController::startFromCurrent(double target)
{
    if (m_reducedMotion) {
        m_engine.cancelAndSettle(m_channel, target);
        return;
    }
    ensureRunning();
    m_engine.startSpringFromCurrent(m_channel,
                                    m_value.value(),
                                    target,
                                    m_value.velocity(),
                                    m_preset);
}

void MotionController::retarget(double target)
{
    if (m_reducedMotion) {
        m_engine.cancelAndSettle(m_channel, target);
        return;
    }
    ensureRunning();
    m_engine.retarget(m_channel, target);
}

void MotionController::adoptVelocity(double velocity)
{
    m_engine.adoptVelocity(m_channel, velocity);
}

void MotionController::cancelAndSettle(double target)
{
    m_engine.cancelAndSettle(m_channel, target);
}

QVariantList MotionController::channelsSnapshot() const
{
    return m_engine.channelsSnapshot();
}

void MotionController::gestureBegin(double initialProgress)
{
    m_gesture.begin(initialProgress);
}

void MotionController::gestureUpdate(double distance, double range)
{
    m_gesture.updateByDistance(distance, range);
    m_value.setValue(m_gesture.progress());
    emit valueChanged();
}

int MotionController::gestureEnd(double velocity,
                                 double forwardThreshold,
                                 double backwardThreshold,
                                 double velocityThreshold)
{
    GestureSettleConfig cfg;
    cfg.forwardThreshold = forwardThreshold;
    cfg.backwardThreshold = backwardThreshold;
    cfg.velocityThreshold = velocityThreshold;
    return m_gesture.end(velocity, cfg);
}

} // namespace Slm::Motion
