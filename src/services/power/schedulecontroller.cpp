#include "schedulecontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

ScheduleController::ScheduleController(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &ScheduleController::tick);
    recoverPendingSchedule();
}

bool ScheduleController::hasPending() const { return m_hasPending; }
QString ScheduleController::action() const { return m_action; }
QString ScheduleController::scheduleMode() const { return m_scheduleMode; }
QDateTime ScheduleController::executeAt() const { return m_executeAt; }

int ScheduleController::secondsRemaining() const
{
    if (!m_hasPending || !m_executeAt.isValid()) {
        return 0;
    }
    return qMax(0, static_cast<int>(QDateTime::currentDateTimeUtc().secsTo(m_executeAt.toUTC())));
}

QString ScheduleController::countdownText() const
{
    if (!m_hasPending) {
        return QString();
    }
    const int seconds = secondsRemaining();
    const int minutes = qMax(1, (seconds + 59) / 60);
    const QString label = m_action.left(1).toUpper() + m_action.mid(1);
    if (minutes >= 60) {
        return QStringLiteral("%1 in %2 hours %3 minutes")
            .arg(label)
            .arg(minutes / 60)
            .arg(minutes % 60);
    }
    return QStringLiteral("%1 in %2 minutes").arg(label).arg(minutes);
}

bool ScheduleController::schedule(const QString &action, const QString &scheduleMode, const QDateTime &executeAt)
{
    const QString normalizedAction = action.trimmed().toLower();
    const QString normalizedMode = scheduleMode.trimmed();
    const QDateTime when = executeAt.toUTC();
    if (normalizedAction.isEmpty() || !when.isValid() || when <= QDateTime::currentDateTimeUtc()) {
        emit scheduleError(QStringLiteral("Invalid scheduled power action."));
        return false;
    }
    m_hasPending = true;
    m_action = normalizedAction;
    m_scheduleMode = normalizedMode.isEmpty() ? QStringLiteral("AtTime") : normalizedMode;
    m_executeAt = when;
    m_createdAt = QDateTime::currentDateTimeUtc();
    m_warnedFiveMinutes = false;
    m_warnedThirtySeconds = false;
    persist();
    updateTimer();
    emit scheduleChanged();
    emit countdownChanged();
    return true;
}

bool ScheduleController::scheduleInMinutes(const QString &action, int minutes)
{
    if (minutes <= 0) {
        return false;
    }
    return schedule(action, QStringLiteral("InDuration"),
                    QDateTime::currentDateTimeUtc().addSecs(minutes * 60));
}

void ScheduleController::cancelSchedule()
{
    if (!m_hasPending) {
        return;
    }
    m_hasPending = false;
    m_action.clear();
    m_scheduleMode.clear();
    m_executeAt = QDateTime();
    m_timer.stop();
    clearPersisted();
    emit scheduleChanged();
    emit countdownChanged();
}

void ScheduleController::recoverPendingSchedule()
{
    QFile file(schedulePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject obj = doc.object();
    const QDateTime when = QDateTime::fromString(obj.value(QStringLiteral("executeAt")).toString(), Qt::ISODate);
    if (!when.isValid() || when.toUTC() <= QDateTime::currentDateTimeUtc()) {
        clearPersisted();
        return;
    }
    m_hasPending = true;
    m_action = obj.value(QStringLiteral("action")).toString();
    m_scheduleMode = obj.value(QStringLiteral("scheduleMode")).toString();
    m_executeAt = when.toUTC();
    m_createdAt = QDateTime::fromString(obj.value(QStringLiteral("createdAt")).toString(), Qt::ISODate).toUTC();
    updateTimer();
    emit scheduleChanged();
    emit countdownChanged();
}

void ScheduleController::tick()
{
    if (!m_hasPending) {
        m_timer.stop();
        return;
    }
    const int remaining = secondsRemaining();
    if (remaining <= 300 && !m_warnedFiveMinutes) {
        m_warnedFiveMinutes = true;
        emit elevatedWarningRequested(m_action, remaining);
    }
    if (remaining <= 30 && !m_warnedThirtySeconds) {
        m_warnedThirtySeconds = true;
        emit blockingWarningRequested(m_action, remaining);
    }
    if (remaining <= 0) {
        const QString actionToRun = m_action;
        cancelSchedule();
        emit executeScheduledActionRequested(actionToRun);
        return;
    }
    emit countdownChanged();
}

QString ScheduleController::schedulePath() const
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(config).filePath(QStringLiteral("slm/power/schedule.json"));
}

void ScheduleController::persist() const
{
    QFileInfo info(schedulePath());
    QDir().mkpath(info.absolutePath());
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("action"), m_action);
    obj.insert(QStringLiteral("scheduleMode"), m_scheduleMode);
    obj.insert(QStringLiteral("executeAt"), m_executeAt.toUTC().toString(Qt::ISODate));
    obj.insert(QStringLiteral("createdAt"), m_createdAt.toUTC().toString(Qt::ISODate));
    obj.insert(QStringLiteral("createdByUser"), true);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void ScheduleController::clearPersisted() const
{
    QFile::remove(schedulePath());
}

void ScheduleController::updateTimer()
{
    if (m_hasPending) {
        m_timer.start();
    } else {
        m_timer.stop();
    }
}
