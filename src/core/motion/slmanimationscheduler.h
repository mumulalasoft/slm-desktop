#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>

namespace Slm::Motion {

class AnimationEngine;

class AnimationScheduler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(double timeScale READ timeScale WRITE setTimeScale NOTIFY timeScaleChanged)
    Q_PROPERTY(qulonglong droppedFrameCount READ droppedFrameCount NOTIFY droppedFrameCountChanged)
    Q_PROPERTY(bool microInteractionSuppressed READ microInteractionSuppressed NOTIFY microInteractionSuppressedChanged)
    Q_PROPERTY(int activeLifecyclePriority READ activeLifecyclePriority NOTIFY activeLifecyclePriorityChanged)

public:
    explicit AnimationScheduler(QObject *parent = nullptr);

    void setEngine(AnimationEngine *engine);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    double timeScale() const;
    void setTimeScale(double scale);
    qulonglong droppedFrameCount() const;
    bool microInteractionSuppressed() const;
    int activeLifecyclePriority() const;

    bool running() const;
    Q_INVOKABLE void beginLifecycle(const QString &owner, int priority);
    Q_INVOKABLE void endLifecycle(const QString &owner);
    Q_INVOKABLE bool canRunPriority(int priority) const;
    Q_INVOKABLE bool shouldCoalesce(const QString &eventKey, int windowMs = 80);

signals:
    void runningChanged();
    void timeScaleChanged();
    void droppedFrameCountChanged();
    void microInteractionSuppressedChanged();
    void activeLifecyclePriorityChanged();
    void frameStepped(double dtSeconds);

private:
    void onFrame();

    AnimationEngine *m_engine = nullptr;
    QElapsedTimer m_timer;
    QTimer m_tick;
    qint64 m_lastMs = 0;
    double m_timeScale = 1.0;
    qulonglong m_droppedFrameCount = 0;
    bool m_running = false;
    bool m_microInteractionSuppressed = false;
    int m_activeLifecyclePriority = 0;
    QHash<QString, int> m_lifecycleOwners;
    QHash<QString, qint64> m_lastEventByKeyMs;
};

} // namespace Slm::Motion
