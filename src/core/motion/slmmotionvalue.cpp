#include "slmmotionvalue.h"

#include <QtGlobal>

namespace Slm::Motion {

MotionValue::MotionValue(QObject *parent)
    : QObject(parent)
{
}

double MotionValue::value() const
{
    return m_value;
}

double MotionValue::velocity() const
{
    return m_velocity;
}

double MotionValue::target() const
{
    return m_target;
}

bool MotionValue::active() const
{
    return m_active;
}

void MotionValue::setValue(double value)
{
    if (qFuzzyCompare(m_value, value)) {
        return;
    }
    m_value = value;
    emit valueChanged();
}

void MotionValue::setVelocity(double velocity)
{
    if (qFuzzyCompare(m_velocity, velocity)) {
        return;
    }
    m_velocity = velocity;
    emit velocityChanged();
}

void MotionValue::setTarget(double target)
{
    if (qFuzzyCompare(m_target, target)) {
        return;
    }
    m_target = target;
    emit targetChanged();
}

void MotionValue::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    emit activeChanged();
}

} // namespace Slm::Motion
