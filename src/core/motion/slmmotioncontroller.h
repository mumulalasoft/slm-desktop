#pragma once

#include "slmanimationengine.h"
#include "slmanimationscheduler.h"
#include "slmgesturebinding.h"
#include "slmmotionvalue.h"

#include <QObject>
#include <QVariantList>

namespace Slm::Motion {

class MotionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool microInteractionSuppressed READ microInteractionSuppressed NOTIFY microInteractionSuppressedChanged)
    Q_PROPERTY(int activeLifecyclePriority READ activeLifecyclePriority NOTIFY activeLifecyclePriorityChanged)
    Q_PROPERTY(double value READ value NOTIFY valueChanged)
    Q_PROPERTY(double velocity READ velocity NOTIFY velocityChanged)
    Q_PROPERTY(double lastFrameDt READ lastFrameDt NOTIFY lastFrameDtChanged)
    Q_PROPERTY(double timeScale READ timeScale WRITE setTimeScale NOTIFY timeScaleChanged)
    Q_PROPERTY(qulonglong droppedFrameCount READ droppedFrameCount NOTIFY droppedFrameCountChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(QString channel READ channel WRITE setChannel NOTIFY channelChanged)
    Q_PROPERTY(QString preset READ preset WRITE setPreset NOTIFY presetChanged)

public:
    enum MotionPriority {
        LowPriority = 0,
        MediumPriority = 1,
        HighPriority = 2
    };
    Q_ENUM(MotionPriority)

    explicit MotionController(QObject *parent = nullptr);

    double value() const;
    double velocity() const;
    double lastFrameDt() const;
    double timeScale() const;
    qulonglong droppedFrameCount() const;
    bool reducedMotion() const;
    bool microInteractionSuppressed() const;
    int activeLifecyclePriority() const;
    QString channel() const;
    QString preset() const;

    void setChannel(const QString &channel);
    void setPreset(const QString &preset);
    void setTimeScale(double scale);
    void setReducedMotion(bool enabled);

    Q_INVOKABLE void ensureRunning();
    Q_INVOKABLE void enableVsyncDriving();
    Q_INVOKABLE void windowFrame();
    Q_INVOKABLE void startFromCurrent(double target);
    Q_INVOKABLE void retarget(double target);
    Q_INVOKABLE void adoptVelocity(double velocity);
    Q_INVOKABLE void cancelAndSettle(double target);
    Q_INVOKABLE QVariantList channelsSnapshot() const;
    Q_INVOKABLE void beginLifecycleTransition(const QString &owner, int priority);
    Q_INVOKABLE void endLifecycleTransition(const QString &owner);
    Q_INVOKABLE bool allowMotionPriority(int priority) const;
    Q_INVOKABLE bool shouldCoalesceEvent(const QString &eventKey, int windowMs = 80);

    Q_INVOKABLE void gestureBegin(double initialProgress = 0.0);
    Q_INVOKABLE void gestureUpdate(double distance, double range);
    Q_INVOKABLE int gestureEnd(double velocity,
                               double forwardThreshold = 0.5,
                               double backwardThreshold = 0.5,
                               double velocityThreshold = 800.0);

signals:
    void valueChanged();
    void velocityChanged();
    void lastFrameDtChanged();
    void timeScaleChanged();
    void droppedFrameCountChanged();
    void reducedMotionChanged();
    void microInteractionSuppressedChanged();
    void activeLifecyclePriorityChanged();
    void channelChanged();
    void presetChanged();

private:
    AnimationEngine m_engine;
    AnimationScheduler m_scheduler;
    GestureBinding m_gesture;
    MotionValue m_value;
    double m_lastFrameDt = 0.0;
    bool m_reducedMotion = false;
    QString m_channel = QStringLiteral("default");
    QString m_preset = QStringLiteral("smooth");
};

} // namespace Slm::Motion
