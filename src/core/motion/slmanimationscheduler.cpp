#include "slmanimationscheduler.h"

#include "slmanimationengine.h"
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
    m_tick.start();
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

bool AnimationScheduler::running() const
{
    return m_running;
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
