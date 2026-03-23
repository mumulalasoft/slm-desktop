#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

namespace Slm::Motion {

class AnimationEngine;

class AnimationScheduler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(double timeScale READ timeScale WRITE setTimeScale NOTIFY timeScaleChanged)
    Q_PROPERTY(qulonglong droppedFrameCount READ droppedFrameCount NOTIFY droppedFrameCountChanged)

public:
    explicit AnimationScheduler(QObject *parent = nullptr);

    void setEngine(AnimationEngine *engine);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    double timeScale() const;
    void setTimeScale(double scale);
    qulonglong droppedFrameCount() const;

    bool running() const;

signals:
    void runningChanged();
    void timeScaleChanged();
    void droppedFrameCountChanged();
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
};

} // namespace Slm::Motion
