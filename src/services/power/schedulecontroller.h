#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>

class ScheduleController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasPending READ hasPending NOTIFY scheduleChanged)
    Q_PROPERTY(QString action READ action NOTIFY scheduleChanged)
    Q_PROPERTY(QString scheduleMode READ scheduleMode NOTIFY scheduleChanged)
    Q_PROPERTY(QDateTime executeAt READ executeAt NOTIFY scheduleChanged)
    Q_PROPERTY(QString countdownText READ countdownText NOTIFY countdownChanged)
    Q_PROPERTY(int secondsRemaining READ secondsRemaining NOTIFY countdownChanged)

public:
    explicit ScheduleController(QObject *parent = nullptr);

    bool hasPending() const;
    QString action() const;
    QString scheduleMode() const;
    QDateTime executeAt() const;
    QString countdownText() const;
    int secondsRemaining() const;

    Q_INVOKABLE bool schedule(const QString &action, const QString &scheduleMode, const QDateTime &executeAt);
    Q_INVOKABLE bool scheduleInMinutes(const QString &action, int minutes);
    Q_INVOKABLE void cancelSchedule();
    Q_INVOKABLE void recoverPendingSchedule();

signals:
    void scheduleChanged();
    void countdownChanged();
    void elevatedWarningRequested(const QString &action, int secondsRemaining);
    void blockingWarningRequested(const QString &action, int secondsRemaining);
    void executeScheduledActionRequested(const QString &action);
    void scheduleError(const QString &message);

private slots:
    void tick();

private:
    QString schedulePath() const;
    void persist() const;
    void clearPersisted() const;
    void updateTimer();

    bool m_hasPending = false;
    QString m_action;
    QString m_scheduleMode;
    QDateTime m_executeAt;
    QDateTime m_createdAt;
    bool m_warnedFiveMinutes = false;
    bool m_warnedThirtySeconds = false;
    QTimer m_timer;
};
