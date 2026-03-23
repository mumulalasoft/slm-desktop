#pragma once

#include "slmmotionpresetlibrary.h"
#include "slmphysicsintegrator.h"
#include "slmspringsolver.h"

#include <QObject>
#include <QHash>
#include <QVariantList>

namespace Slm::Motion {

class AnimationEngine : public QObject
{
    Q_OBJECT

public:
    explicit AnimationEngine(QObject *parent = nullptr);

    Q_INVOKABLE void startSpringFromCurrent(const QString &channel,
                                            double current,
                                            double target,
                                            double velocity,
                                            const QString &presetName = QStringLiteral("smooth"));
    Q_INVOKABLE void retarget(const QString &channel, double target);
    Q_INVOKABLE void adoptVelocity(const QString &channel, double velocity);
    Q_INVOKABLE void cancelAndSettle(const QString &channel, double target);
    Q_INVOKABLE bool isActive(const QString &channel) const;
    Q_INVOKABLE double value(const QString &channel) const;
    Q_INVOKABLE QVariantList channelsSnapshot() const;

    void tick(double dtSeconds);

signals:
    void channelUpdated(const QString &channel, double value, double velocity, bool active);

private:
    struct ChannelState {
        SpringSolver::State springState;
        PhysicsIntegrator::State physicsState;
        MotionPresetSpec spec;
        double target = 0.0;
        bool springActive = false;
        bool physicsActive = false;
    };

    QHash<QString, ChannelState> m_channels;
};

} // namespace Slm::Motion
