#include "recoveryservice.h"

#include <QDBusConnection>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr const char kApiVersion[] = "1.0";
constexpr int kCrashLoopThreshold = 3;
constexpr int kCrashWindowSeconds = 10;
constexpr int kMaxAutoRecoveryAttempts = 2;

QStringList criticalProbes()
{
    return {
        QStringLiteral("compositor"),
        QStringLiteral("shell"),
        QStringLiteral("login"),
        QStringLiteral("gpu-wayland"),
        QStringLiteral("network"),
        QStringLiteral("portal"),
        QStringLiteral("greeter"),
    };
}

QString recoveryPartitionRequestPath()
{
    return Slm::Login::ConfigManager::configDir() + QStringLiteral("/recovery-partition-request.json");
}
}

RecoveryService::RecoveryService(QObject *parent)
    : QObject(parent)
{
}

RecoveryService::~RecoveryService()
{
    if (m_serviceRegistered) {
        Slm::Recovery::Dbus::unregisterServiceObject(QDBusConnection::sessionBus());
    }
}

bool RecoveryService::start(QString *error)
{
    if (error) {
        error->clear();
    }

    m_serviceRegistered = Slm::Recovery::Dbus::registerServiceObject(QDBusConnection::sessionBus(), this);
    if (!m_serviceRegistered && error) {
        *error = QStringLiteral("dbus-register-failed");
    }
    emit serviceRegisteredChanged();
    return m_serviceRegistered;
}

bool RecoveryService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString RecoveryService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap RecoveryService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), Slm::Recovery::Dbus::serviceName()},
        {QStringLiteral("apiVersion"), apiVersion()},
        {QStringLiteral("registered"), m_serviceRegistered},
    };
}

QVariantMap RecoveryService::GetStatus() const
{
    return composeStatus();
}

QVariantMap RecoveryService::ReportHealth(const QVariantMap &probe)
{
    const QString component = normalizeComponent(probe.value(QStringLiteral("component")).toString());
    if (component.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("component-empty")},
        };
    }

    ProbeState state;
    state.ok = probe.value(QStringLiteral("ok"), true).toBool();
    state.detail = probe.value(QStringLiteral("detail")).toString().trimmed();
    state.updatedAt = QDateTime::currentDateTimeUtc();
    m_probeStates.insert(component, state);

    QVariantMap status = composeStatus(QStringLiteral("health-update"));
    emit RecoveryStateChanged(status);
    return status;
}

QVariantMap RecoveryService::ReportCrash(const QString &component, const QString &reason)
{
    const QString normalized = normalizeComponent(component);
    if (normalized.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("component-empty")},
        };
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QList<QDateTime> history = m_crashHistoryByComponent.value(normalized);
    history.append(now);
    m_crashHistoryByComponent.insert(normalized, history);
    pruneCrashHistory(normalized, now);

    const int crashCount = crashCountInWindow(normalized);
    if ((normalized == QLatin1String("compositor") || normalized == QLatin1String("shell"))
            && crashCount > kCrashLoopThreshold) {
        return TriggerAutoRecovery(QStringLiteral("%1-crash-loop: %2")
                                   .arg(normalized, reason.trimmed()));
    }

    QVariantMap status = composeStatus(QStringLiteral("crash-report"));
    status.insert(QStringLiteral("component"), normalized);
    status.insert(QStringLiteral("crashCount10s"), crashCount);
    emit RecoveryStateChanged(status);
    return status;
}

QVariantMap RecoveryService::TriggerAutoRecovery(const QString &reason)
{
    const QString why = reason.trimmed().isEmpty()
            ? QStringLiteral("auto-recovery-trigger")
            : reason.trimmed();
    m_lastReason = why;

    QString rollbackError;
    bool rollbackOk = false;
    if (m_config.hasPreviousConfig()) {
        rollbackOk = m_config.rollbackToPrevious(&rollbackError);
    }
    if (!rollbackOk) {
        rollbackOk = m_config.rollbackToSafe(&rollbackError);
    }

    ++m_autoRecoveryAttempts;

    QString safeModeError;
    const bool safeModeOk = setSafeModeForced(true, why, &safeModeError);
    if (safeModeOk) {
        emit SafeModeRequested(why);
    }

    const bool shouldEscalatePartition = (!safeModeOk || m_autoRecoveryAttempts >= kMaxAutoRecoveryAttempts);
    bool partitionOk = false;
    QString partitionError;
    if (shouldEscalatePartition) {
        partitionOk = writeRecoveryPartitionRequest(why, &partitionError);
        if (partitionOk) {
            m_recoveryPartitionRequested = true;
            emit RecoveryPartitionRequested(why);
        }
    }

    QVariantMap status = composeStatus(QStringLiteral("auto-recovery"));
    status.insert(QStringLiteral("reason"), why);
    status.insert(QStringLiteral("rollbackOk"), rollbackOk);
    status.insert(QStringLiteral("rollbackError"), rollbackError);
    status.insert(QStringLiteral("safeModeRequested"), safeModeOk);
    status.insert(QStringLiteral("safeModeError"), safeModeError);
    status.insert(QStringLiteral("partitionRequested"), shouldEscalatePartition && partitionOk);
    status.insert(QStringLiteral("partitionError"), partitionError);
    emit RecoveryStateChanged(status);
    return status;
}

QVariantMap RecoveryService::RequestSafeMode(const QString &reason)
{
    QString error;
    const QString why = reason.trimmed().isEmpty()
            ? QStringLiteral("manual-safe-mode")
            : reason.trimmed();
    const bool ok = setSafeModeForced(true, why, &error);
    if (ok) {
        m_lastReason = why;
        emit SafeModeRequested(why);
    }
    QVariantMap status = composeStatus(QStringLiteral("request-safe-mode"));
    status.insert(QStringLiteral("ok"), ok);
    status.insert(QStringLiteral("error"), error);
    emit RecoveryStateChanged(status);
    return status;
}

QVariantMap RecoveryService::RequestRecoveryPartition(const QString &reason)
{
    QString error;
    const QString why = reason.trimmed().isEmpty()
            ? QStringLiteral("manual-recovery-partition")
            : reason.trimmed();
    const bool ok = writeRecoveryPartitionRequest(why, &error);
    QString bootloaderError;
    bool bootloaderTriggered = false;
    if (ok) {
        bootloaderTriggered = triggerBootloaderRecovery(why, &bootloaderError);
    }
    if (ok) {
        m_recoveryPartitionRequested = true;
        m_lastReason = why;
        emit RecoveryPartitionRequested(why);
    }
    QVariantMap status = composeStatus(QStringLiteral("request-recovery-partition"));
    status.insert(QStringLiteral("ok"), ok);
    status.insert(QStringLiteral("error"), error);
    status.insert(QStringLiteral("bootloaderTriggered"), bootloaderTriggered);
    status.insert(QStringLiteral("bootloaderError"), bootloaderError);
    emit RecoveryStateChanged(status);
    return status;
}

QVariantMap RecoveryService::ClearRecoveryFlags()
{
    QString safeError;
    const bool safeOk = setSafeModeForced(false, QString(), &safeError);

    QString fileError;
    const QString markerPath = recoveryPartitionRequestPath();
    bool markerCleared = true;
    if (QFileInfo::exists(markerPath)) {
        markerCleared = QFile::remove(markerPath);
        if (!markerCleared) {
            fileError = QStringLiteral("cannot-remove-recovery-partition-request");
        }
    }

    if (safeOk && markerCleared) {
        m_recoveryPartitionRequested = false;
        m_autoRecoveryAttempts = 0;
        m_lastReason.clear();
    }

    QVariantMap status = composeStatus(QStringLiteral("clear-flags"));
    status.insert(QStringLiteral("ok"), safeOk && markerCleared);
    status.insert(QStringLiteral("safeModeError"), safeError);
    status.insert(QStringLiteral("partitionError"), fileError);
    emit RecoveryStateChanged(status);
    return status;
}

QString RecoveryService::normalizeComponent(const QString &value)
{
    return value.trimmed().toLower();
}

bool RecoveryService::isCriticalProbe(const QString &component)
{
    return criticalProbes().contains(component.trimmed().toLower());
}

void RecoveryService::pruneCrashHistory(const QString &component, const QDateTime &nowUtc)
{
    QList<QDateTime> history = m_crashHistoryByComponent.value(component);
    QList<QDateTime> kept;
    kept.reserve(history.size());

    for (const QDateTime &eventTs : history) {
        if (eventTs.isValid() && eventTs.secsTo(nowUtc) <= kCrashWindowSeconds) {
            kept.append(eventTs);
        }
    }

    m_crashHistoryByComponent.insert(component, kept);
}

int RecoveryService::crashCountInWindow(const QString &component) const
{
    return m_crashHistoryByComponent.value(component).size();
}

bool RecoveryService::setSafeModeForced(bool enabled, const QString &reason, QString *error) const
{
    if (error) {
        error->clear();
    }

    Slm::Login::SessionState state;
    QString loadError;
    if (!Slm::Login::SessionStateIO::load(state, loadError)) {
        if (error) {
            *error = loadError;
        }
        return false;
    }

    state.safeModeForced = enabled;
    state.recoveryReason = enabled ? reason.trimmed() : QString();
    state.lastUpdated = QDateTime::currentDateTimeUtc();

    QString saveError;
    if (!Slm::Login::SessionStateIO::save(state, saveError)) {
        if (error) {
            *error = saveError;
        }
        return false;
    }

    return true;
}

bool RecoveryService::writeRecoveryPartitionRequest(const QString &reason, QString *error) const
{
    if (error) {
        error->clear();
    }

    const QString path = recoveryPartitionRequestPath();
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        if (error) {
            *error = QStringLiteral("cannot-create-recovery-dir");
        }
        return false;
    }

    QJsonObject payload{
        {QStringLiteral("requested"), true},
        {QStringLiteral("reason"), reason.trimmed()},
        {QStringLiteral("requestedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("host"), QCoreApplication::applicationName()},
    };

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("cannot-open-recovery-request");
        }
        return false;
    }

    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        file.cancelWriting();
        if (error) {
            *error = QStringLiteral("cannot-write-recovery-request");
        }
        return false;
    }

    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("cannot-commit-recovery-request");
        }
        return false;
    }

    return true;
}

bool RecoveryService::triggerBootloaderRecovery(const QString &reason, QString *error) const
{
    if (error) {
        error->clear();
    }

    const QString helper = qEnvironmentVariable("SLM_RECOVERY_BOOTLOADER_HELPER").trimmed();
    if (helper.isEmpty()) {
        return false;
    }
    if (!QFileInfo::exists(helper)) {
        if (error) {
            *error = QStringLiteral("bootloader-helper-not-found");
        }
        return false;
    }
    if (!QFileInfo(helper).isExecutable()) {
        if (error) {
            *error = QStringLiteral("bootloader-helper-not-executable");
        }
        return false;
    }

    const QString entryHint = qEnvironmentVariable("SLM_RECOVERY_BOOT_ENTRY", QStringLiteral("recovery")).trimmed();
    const QString arg = entryHint.isEmpty() ? QStringLiteral("recovery") : entryHint;
    QProcess proc;
    proc.setProgram(helper);
    proc.setArguments(QStringList{arg});
    proc.start();
    if (!proc.waitForFinished(8000)) {
        proc.kill();
        if (error) {
            *error = QStringLiteral("bootloader-helper-timeout");
        }
        return false;
    }
    if (proc.exitCode() != 0) {
        if (error) {
            const QString stderr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            *error = stderr.isEmpty()
                    ? QStringLiteral("bootloader-helper-exit-%1").arg(proc.exitCode())
                    : stderr;
        }
        return false;
    }

    Q_UNUSED(reason)
    return true;
}

QVariantMap RecoveryService::composeStatus(const QString &action) const
{
    int unhealthyCriticalCount = 0;
    QVariantMap probes;
    for (auto it = m_probeStates.constBegin(); it != m_probeStates.constEnd(); ++it) {
        const ProbeState &probe = it.value();
        probes.insert(it.key(), QVariantMap{
            {QStringLiteral("ok"), probe.ok},
            {QStringLiteral("detail"), probe.detail},
            {QStringLiteral("updatedAt"), probe.updatedAt.toString(Qt::ISODate)},
            {QStringLiteral("critical"), isCriticalProbe(it.key())},
        });
        if (!probe.ok && isCriticalProbe(it.key())) {
            ++unhealthyCriticalCount;
        }
    }

    QVariantMap crashStats;
    for (auto it = m_crashHistoryByComponent.constBegin(); it != m_crashHistoryByComponent.constEnd(); ++it) {
        crashStats.insert(it.key(), it.value().size());
    }

    QVariantMap state{
        {QStringLiteral("ok"), true},
        {QStringLiteral("action"), action},
        {QStringLiteral("apiVersion"), apiVersion()},
        {QStringLiteral("serviceRegistered"), m_serviceRegistered},
        {QStringLiteral("autoRecoveryAttempts"), m_autoRecoveryAttempts},
        {QStringLiteral("recoveryPartitionRequested"), m_recoveryPartitionRequested},
        {QStringLiteral("lastReason"), m_lastReason},
        {QStringLiteral("bootloaderHelper"),
         qEnvironmentVariable("SLM_RECOVERY_BOOTLOADER_HELPER")},
        {QStringLiteral("unhealthyCriticalProbes"), unhealthyCriticalCount},
        {QStringLiteral("probes"), probes},
        {QStringLiteral("crashCounts10s"), crashStats},
    };

    Slm::Login::SessionState sessionState;
    QString loadError;
    if (Slm::Login::SessionStateIO::load(sessionState, loadError)) {
        state.insert(QStringLiteral("safeModeForced"), sessionState.safeModeForced);
        state.insert(QStringLiteral("sessionRecoveryReason"), sessionState.recoveryReason);
        state.insert(QStringLiteral("sessionCrashCount"), sessionState.crashCount);
    } else {
        state.insert(QStringLiteral("safeModeForced"), false);
        state.insert(QStringLiteral("sessionStateError"), loadError);
    }

    return state;
}
