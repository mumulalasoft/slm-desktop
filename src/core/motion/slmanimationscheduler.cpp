#include "slmanimationscheduler.h"

#include "slmanimationengine.h"
#include <QDateTime>
#include <QtMath>

namespace Slm::Motion {

AnimationScheduler::AnimationScheduler(QObject *parent)
    : QObject(parent)
{
    m_tick.setTimerType(Qt::PreciseTimer);
    m_tick.setInterval(16);
    connect(&m_tick, &QTimer::timeout, this, [this]() { onFrame(); });
}

void AnimationScheduler::setEngine(AnimationEngine *engine)
{
    m_engine = engine;
}

void AnimationScheduler::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_timer.restart();
    m_lastMs = 0;
    if (!m_externalDriving) {
        m_tick.start();
    }
    emit runningChanged();
}

void AnimationScheduler::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_tick.stop();
    emit runningChanged();
}

void AnimationScheduler::setExternalDriving(bool enabled)
{
    if (m_externalDriving == enabled) {
        return;
    }
    m_externalDriving = enabled;
    if (m_running) {
        if (enabled) {
            m_tick.stop();
        } else {
            m_lastMs = m_timer.elapsed();
            m_tick.start();
        }
    }
}

bool AnimationScheduler::externalDriving() const
{
    return m_externalDriving;
}

void AnimationScheduler::windowFrame()
{
    if (!m_running || !m_externalDriving) {
        return;
    }
    onFrame();
}

double AnimationScheduler::timeScale() const
{
    return m_timeScale;
}

void AnimationScheduler::setTimeScale(double scale)
{
    const double clamped = qBound(0.05, scale, 2.0);
    if (qFuzzyCompare(1.0 + m_timeScale, 1.0 + clamped)) {
        return;
    }
    m_timeScale = clamped;
    emit timeScaleChanged();
}

qulonglong AnimationScheduler::droppedFrameCount() const
{
    return m_droppedFrameCount;
}

bool AnimationScheduler::microInteractionSuppressed() const
{
    return m_microInteractionSuppressed;
}

int AnimationScheduler::activeLifecyclePriority() const
{
    return m_activeLifecyclePriority;
}

bool AnimationScheduler::running() const
{
    return m_running;
}

void AnimationScheduler::beginLifecycle(const QString &owner, int priority)
{
    const QString key = owner.trimmed().isEmpty() ? QStringLiteral("__anonymous__") : owner.trimmed();
    const int normalized = qBound(0, priority, 2);
    m_lifecycleOwners.insert(key, normalized);

    int nextPriority = 0;
    for (auto it = m_lifecycleOwners.cbegin(); it != m_lifecycleOwners.cend(); ++it) {
        nextPriority = qMax(nextPriority, it.value());
    }

    if (nextPriority != m_activeLifecyclePriority) {
        m_activeLifecyclePriority = nextPriority;
        emit activeLifecyclePriorityChanged();
    }

    const bool nextSuppressed = m_activeLifecyclePriority >= 1;
    if (nextSuppressed != m_microInteractionSuppressed) {
        m_microInteractionSuppressed = nextSuppressed;
        emit microInteractionSuppressedChanged();
    }
}

void AnimationScheduler::endLifecycle(const QString &owner)
{
    const QString key = owner.trimmed().isEmpty() ? QStringLiteral("__anonymous__") : owner.trimmed();
    m_lifecycleOwners.remove(key);

    int nextPriority = 0;
    for (auto it = m_lifecycleOwners.cbegin(); it != m_lifecycleOwners.cend(); ++it) {
        nextPriority = qMax(nextPriority, it.value());
    }

    if (nextPriority != m_activeLifecyclePriority) {
        m_activeLifecyclePriority = nextPriority;
        emit activeLifecyclePriorityChanged();
    }

    const bool nextSuppressed = m_activeLifecyclePriority >= 1;
    if (nextSuppressed != m_microInteractionSuppressed) {
        m_microInteractionSuppressed = nextSuppressed;
        emit microInteractionSuppressedChanged();
    }
}

bool AnimationScheduler::canRunPriority(int priority) const
{
    const int normalized = qBound(0, priority, 2);
    if (m_activeLifecyclePriority <= 0) {
        return true;
    }
    return normalized >= m_activeLifecyclePriority;
}

bool AnimationScheduler::shouldCoalesce(const QString &eventKey, int windowMs)
{
    const QString key = eventKey.trimmed();
    if (key.isEmpty()) {
        return false;
    }
    const int normalizedWindowMs = qMax(1, windowMs);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastMs = m_lastEventByKeyMs.value(key, 0);
    m_lastEventByKeyMs.insert(key, nowMs);
    return lastMs > 0 && (nowMs - lastMs) < normalizedWindowMs;
}

void AnimationScheduler::onFrame()
{
    const qint64 now = m_timer.elapsed();
    const qint64 deltaMs = m_lastMs == 0 ? 16 : qMax<qint64>(1, now - m_lastMs);
    m_lastMs = now;
    const int dropped = qMax(0, static_cast<int>(qFloor(static_cast<double>(deltaMs) / 16.67)) - 1);
    if (dropped > 0) {
        m_droppedFrameCount += static_cast<qulonglong>(dropped);
        emit droppedFrameCountChanged();
    }

    const double dtSeconds = qBound(0.001, (static_cast<double>(deltaMs) / 1000.0) * m_timeScale, 0.05);

    emit frameStepped(dtSeconds);
    if (m_engine) {
        m_engine->tick(dtSeconds);
    }
}

} // namespace Slm::Motion
