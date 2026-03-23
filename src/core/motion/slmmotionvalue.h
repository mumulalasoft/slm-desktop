#pragma once

#include <QObject>

namespace Slm::Motion {

class MotionValue : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double velocity READ velocity WRITE setVelocity NOTIFY velocityChanged)
    Q_PROPERTY(double target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit MotionValue(QObject *parent = nullptr);

    double value() const;
    double velocity() const;
    double target() const;
    bool active() const;

    void setValue(double value);
    void setVelocity(double velocity);
    void setTarget(double target);
    void setActive(bool active);

signals:
    void valueChanged();
    void velocityChanged();
    void targetChanged();
    void activeChanged();

private:
    double m_value = 0.0;
    double m_velocity = 0.0;
    double m_target = 0.0;
    bool m_active = false;
};

} // namespace Slm::Motion
