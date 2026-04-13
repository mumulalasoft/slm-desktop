#include "recoveryapp.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"

namespace Slm::Login {

namespace {

QJsonObject readJsonObjectFile(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }
    return doc.object();
}

QString toDisplayString(const QJsonValue &value)
{
    if (value.isUndefined()) {
        return QStringLiteral("(missing)");
    }
    if (value.isNull()) {
        return QStringLiteral("null");
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return QStringLiteral("(unsupported)");
}

} // namespace

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

bool RecoveryApp::restartDesktop()
{
    qInfo("slm-recovery-app: restart desktop requested");
    QCoreApplication::quit();
    return true;
}

bool RecoveryApp::disableExtensions()
{
    const bool ok = writeFlagFile(QStringLiteral("disable-extensions.flag"));
    if (!ok) {
        qWarning("slm-recovery-app: disableExtensions failed");
        return false;
    }
    qInfo("slm-recovery-app: disable extensions flag written");
    return true;
}

bool RecoveryApp::resetGraphicsStack()
{
    // Force next session onto software rendering profile.
    const bool ok1 = writeFlagFile(QStringLiteral("force-software-rendering.flag"));
    const bool ok2 = writeFlagFile(QStringLiteral("disable-effects.flag"));
    if (!ok1 || !ok2) {
        qWarning("slm-recovery-app: resetGraphicsStack failed");
        return false;
    }
    qInfo("slm-recovery-app: graphics reset flags written");
    return true;
}

bool RecoveryApp::openTerminal()
{
    const QStringList candidates{
        QStringLiteral("x-terminal-emulator"),
        QStringLiteral("konsole"),
        QStringLiteral("gnome-terminal"),
        QStringLiteral("xfce4-terminal"),
        QStringLiteral("xterm"),
    };

    for (const QString &bin : candidates) {
        if (QStandardPaths::findExecutable(bin).isEmpty()) {
            continue;
        }
        if (QProcess::startDetached(bin, {})) {
            qInfo("slm-recovery-app: terminal started via %s", qUtf8Printable(bin));
            return true;
        }
    }

    qWarning("slm-recovery-app: openTerminal failed (no terminal binary)");
    return false;
}

bool RecoveryApp::networkRepair()
{
    // Best-effort in user session: turn networking/radios on.
    if (!QStandardPaths::findExecutable(QStringLiteral("nmcli")).isEmpty()) {
        QProcess nmOn;
        nmOn.start(QStringLiteral("nmcli"), {QStringLiteral("networking"), QStringLiteral("on")});
        nmOn.waitForFinished(2000);
        QProcess radioOn;
        radioOn.start(QStringLiteral("nmcli"), {QStringLiteral("radio"), QStringLiteral("all"), QStringLiteral("on")});
        radioOn.waitForFinished(2000);
        const bool ok = (nmOn.exitCode() == 0 && radioOn.exitCode() == 0);
        qInfo("slm-recovery-app: networkRepair nmcli result=%s", ok ? "ok" : "partial");
        return ok;
    }

    // Fallback (may require permission, still useful when available).
    if (QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty()) {
        qWarning("slm-recovery-app: networkRepair failed (no nmcli/systemctl)");
        return false;
    }
    QProcess p;
    p.start(QStringLiteral("systemctl"), {QStringLiteral("restart"), QStringLiteral("NetworkManager.service")});
    if (!p.waitForFinished(3000)) {
        p.kill();
        qWarning("slm-recovery-app: networkRepair timeout");
        return false;
    }
    return p.exitCode() == 0;
}

bool RecoveryApp::repairSystem()
{
    // Optional external helper for distro-specific repair flow.
    const QString helper = qEnvironmentVariable(QStringLiteral("SLM_RECOVERY_REPAIR_HELPER")).trimmed();
    if (!helper.isEmpty()) {
        if (!QFileInfo::exists(helper) || !QFileInfo(helper).isExecutable()) {
            qWarning("slm-recovery-app: repairSystem helper invalid: %s", qUtf8Printable(helper));
            return false;
        }
        QProcess p;
        p.start(helper, {});
        if (!p.waitForFinished(30000)) {
            p.kill();
            qWarning("slm-recovery-app: repairSystem helper timeout");
            return false;
        }
        return p.exitCode() == 0;
    }

    // Generic fallback: non-interactive filesystem preen.
    if (QStandardPaths::findExecutable(QStringLiteral("fsck")).isEmpty()) {
        qWarning("slm-recovery-app: repairSystem failed (fsck not found)");
        return false;
    }
    QProcess fsck;
    fsck.start(QStringLiteral("fsck"), {QStringLiteral("-A"), QStringLiteral("-T"), QStringLiteral("-p")});
    if (!fsck.waitForFinished(30000)) {
        fsck.kill();
        qWarning("slm-recovery-app: repairSystem fsck timeout");
        return false;
    }
    const int code = fsck.exitCode();
    // fsck exit code 0/1/2 are generally success with or without fixes.
    const bool ok = (code == 0 || code == 1 || code == 2);
    if (!ok) {
        qWarning("slm-recovery-app: repairSystem fsck exit=%d", code);
    }
    return ok;
}

bool RecoveryApp::reinstallSystem()
{
    // Optional external helper for installer/reimage integration.
    const QString helper = qEnvironmentVariable(QStringLiteral("SLM_RECOVERY_REINSTALL_HELPER")).trimmed();
    if (!helper.isEmpty()) {
        if (!QFileInfo::exists(helper) || !QFileInfo(helper).isExecutable()) {
            qWarning("slm-recovery-app: reinstallSystem helper invalid: %s", qUtf8Printable(helper));
            return false;
        }
        QProcess p;
        p.start(helper, {});
        if (!p.waitForFinished(30000)) {
            p.kill();
            qWarning("slm-recovery-app: reinstallSystem helper timeout");
            return false;
        }
        return p.exitCode() == 0;
    }

    const QByteArray payload = QByteArrayLiteral("requested=1\nrequestedAt=")
                               + QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8()
                               + QByteArrayLiteral("\n");
    const bool ok = writeFlagFile(QStringLiteral("reinstall-system-request.flag"), payload);
    if (!ok) {
        qWarning("slm-recovery-app: reinstallSystem failed writing request flag");
        return false;
    }
    qInfo("slm-recovery-app: reinstallSystem request flag written");
    return true;
}

QVariantMap RecoveryApp::previewSnapshotDiff(const QString &snapshotId) const
{
    QVariantMap out{
        {QStringLiteral("snapshotId"), snapshotId},
        {QStringLiteral("exists"), false},
        {QStringLiteral("rowCount"), 0},
        {QStringLiteral("changedCount"), 0},
        {QStringLiteral("rows"), QVariantList{}},
    };

    const QString path = ConfigManager::snapshotPath(snapshotId);
    if (!QFile::exists(path)) {
        out.insert(QStringLiteral("error"),
                   QStringLiteral("snapshot not found: ") + snapshotId);
        return out;
    }
    out.insert(QStringLiteral("exists"), true);

    ConfigManager config;
    config.load();
    const QJsonObject active = config.currentConfig();
    const QJsonObject snapshot = readJsonObjectFile(path);
    QSet<QString> keys;
    for (auto it = active.begin(); it != active.end(); ++it) {
        keys.insert(it.key());
    }
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        keys.insert(it.key());
    }

    QStringList orderedKeys = keys.values();
    orderedKeys.sort();

    QVariantList rows;
    int changedCount = 0;
    for (const QString &key : orderedKeys) {
        const QJsonValue a = active.value(key);
        const QJsonValue b = snapshot.value(key);
        const bool inActive = !a.isUndefined();
        const bool inSnapshot = !b.isUndefined();
        QString status;
        if (inActive && inSnapshot) {
            status = (a == b) ? QStringLiteral("same") : QStringLiteral("changed");
        } else if (inActive) {
            status = QStringLiteral("removed");
        } else {
            status = QStringLiteral("added");
        }
        if (status != QLatin1String("same")) {
            ++changedCount;
        }

        rows.append(QVariantMap{
            {QStringLiteral("key"), key},
            {QStringLiteral("status"), status},
            {QStringLiteral("active"), toDisplayString(a)},
            {QStringLiteral("snapshot"), toDisplayString(b)},
        });
    }

    out.insert(QStringLiteral("rows"), rows);
    out.insert(QStringLiteral("rowCount"), rows.size());
    out.insert(QStringLiteral("changedCount"), changedCount);
    return out;
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

QVariantMap RecoveryApp::daemonHealthSnapshot() const
{
    QDBusInterface iface(QStringLiteral("org.slm.WorkspaceManager"),
                         QStringLiteral("/org/slm/WorkspaceManager"),
                         QStringLiteral("org.slm.WorkspaceManager1"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }

    const QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("DaemonHealthSnapshot"));
    if (!reply.isValid()) {
        return {};
    }
    return reply.value();
}

void RecoveryApp::exitToDesktop()
{
    qInfo("slm-recovery-app: user requested exit to desktop");
    QCoreApplication::quit();
}

bool RecoveryApp::writeFlagFile(const QString &name, const QByteArray &value) const
{
    const QString flagDir = ConfigManager::configDir() + QStringLiteral("/flags");
    if (!QDir().mkpath(flagDir)) {
        return false;
    }

    QSaveFile file(flagDir + QStringLiteral("/") + name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (file.write(value) != value.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace Slm::Login
