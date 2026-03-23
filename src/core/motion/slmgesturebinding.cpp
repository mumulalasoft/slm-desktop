#include "slmgesturebinding.h"

#include <QtMath>

namespace Slm::Motion {

GestureBinding::GestureBinding(QObject *parent)
    : QObject(parent)
{
}

void GestureBinding::begin(double initialProgress)
{
    const double bounded = qBound(0.0, initialProgress, 1.0);
    if (!qFuzzyCompare(1.0 + bounded, 1.0 + m_progress)) {
        m_progress = bounded;
        emit progressChanged();
    }
}

void GestureBinding::updateByDistance(double distance, double range)
{
    if (qFuzzyIsNull(range)) {
        return;
    }
    const double p = qBound(0.0, distance / range, 1.0);
    if (!qFuzzyCompare(1.0 + p, 1.0 + m_progress)) {
        m_progress = p;
        emit progressChanged();
    }
}

int GestureBinding::end(double velocity, const GestureSettleConfig &cfg)
{
    if (velocity >= cfg.velocityThreshold || m_progress >= cfg.forwardThreshold) {
        return static_cast<int>(SettleDecision::Forward);
    }
    if (velocity <= -cfg.velocityThreshold || m_progress <= (1.0 - cfg.backwardThreshold)) {
        return static_cast<int>(SettleDecision::Backward);
    }
    return static_cast<int>(SettleDecision::Stay);
}

double GestureBinding::progress() const
{
    return m_progress;
}

} // namespace Slm::Motion
