#include "recoveryapp.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>

#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"

namespace Slm::Login {

RecoveryApp::RecoveryApp(QObject *parent)
    : QObject(parent)
{
    // Load session state so QML properties are populated at startup.
    SessionState state;
    QString err;
    SessionStateIO::load(state, err);

    m_recoveryReason  = state.recoveryReason;
    m_lastBootStatus  = state.lastBootStatus;
    m_crashCount      = state.crashCount;

    ConfigManager config;
    config.load();
    m_hasPreviousConfig = config.hasPreviousConfig();

    // Enumerate snapshots (newest first).
    const QString snapDir = ConfigManager::snapshotDir();
    const QDir dir(snapDir);
    QStringList entries = dir.entryList(
        QStringList{QStringLiteral("*.json")},
        QDir::Files, QDir::Time);
    // Strip .json suffix to expose snapshot ids.
    for (const QString &entry : entries)
        m_snapshots.append(QFileInfo(entry).baseName());
}

// ── Properties ────────────────────────────────────────────────────────────────

QString RecoveryApp::recoveryReason()    const { return m_recoveryReason; }
QString RecoveryApp::lastBootStatus()    const { return m_lastBootStatus; }
int     RecoveryApp::crashCount()        const { return m_crashCount; }
bool    RecoveryApp::hasPreviousConfig() const { return m_hasPreviousConfig; }
QStringList RecoveryApp::availableSnapshots() const { return m_snapshots; }

// ── Invokables ────────────────────────────────────────────────────────────────

bool RecoveryApp::resetToSafeDefaults()
{
    ConfigManager config;
    config.load();
    QString err;
    if (!config.rollbackToSafe(&err)) {
        qWarning("slm-recovery-app: resetToSafeDefaults failed: %s", qUtf8Printable(err));
        return false;
    }
    qInfo("slm-recovery-app: config reset to safe defaults");
    return true;
}

bool RecoveryApp::rollbackToPrevious()
{
    ConfigManager config;
    config.load();
    QString err;
    if (!config.rollbackToPrevious(&err)) {
        qWarning("slm-recovery-app: rollbackToPrevious failed: %s", qUtf8Printable(err));
        return false;
    }
    qInfo("slm-recovery-app: config rolled back to previous");
    return true;
}

bool RecoveryApp::restoreSnapshot(const QString &snapshotId)
{
    ConfigManager config;
    config.load();
    QString err;
    if (!config.restoreSnapshot(snapshotId, &err)) {
        qWarning("slm-recovery-app: restoreSnapshot('%s') failed: %s",
                 qUtf8Printable(snapshotId), qUtf8Printable(err));
        return false;
    }
    qInfo("slm-recovery-app: snapshot '%s' restored", qUtf8Printable(snapshotId));
    return true;
}

bool RecoveryApp::factoryReset()
{
    ConfigManager config;
    config.load();
    QString err;
    if (!config.factoryReset(&err)) {
        qWarning("slm-recovery-app: factoryReset failed: %s", qUtf8Printable(err));
        return false;
    }
    qInfo("slm-recovery-app: factory reset complete");
    return true;
}

QString RecoveryApp::logSummary() const
{
    // Attempt to pull recent slm-related journal entries.
    QProcess proc;
    proc.start(QStringLiteral("journalctl"),
               {QStringLiteral("-n"), QStringLiteral("50"),
                QStringLiteral("--no-pager"),
                QStringLiteral("-u"), QStringLiteral("greetd"),
                QStringLiteral("--output"), QStringLiteral("short-iso")});
    if (!proc.waitForFinished(3000)) {
        return QStringLiteral("(journalctl not available)");
    }
    const QString output = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
    return output.isEmpty() ? QStringLiteral("(no log entries found)") : output;
}

void RecoveryApp::exitToDesktop()
{
    qInfo("slm-recovery-app: user requested exit to desktop");
    QCoreApplication::quit();
}

} // namespace Slm::Login
