#include "dbusinterface.h"

#include "../../apps/settings/modules/developer/envvalidator.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QFile>

static constexpr char kServiceName[] = "org.slm.Environment1";
static constexpr char kObjectPath[]  = "/org/slm/Environment";

// ── Sandbox detection ─────────────────────────────────────────────────────────
//
// We check two signals:
//   1. /proc/<pid>/cgroup contains "flatpak" — reliable for Flatpak on cgroup v1/v2
//   2. /proc/<pid>/exe resolves to a path under /flatpak/ or /snap/  — snap fallback
//
// If Flatpak is detected we also read FLATPAK_ID from /proc/<pid>/environ to
// obtain the app ID for the per-app ownership check.
//
struct SandboxInfo {
    bool    sandboxed = false;
    QString appId;          // FLATPAK_ID if Flatpak, empty otherwise
};

static SandboxInfo probeProcess(uint pid)
{
    // 1. cgroup check (Flatpak)
    QFile cgroup(QStringLiteral("/proc/%1/cgroup").arg(pid));
    if (cgroup.open(QIODevice::ReadOnly)) {
        if (cgroup.readAll().contains("flatpak")) {
            // Read FLATPAK_ID from environment
            QFile envFile(QStringLiteral("/proc/%1/environ").arg(pid));
            if (envFile.open(QIODevice::ReadOnly)) {
                for (const QByteArray &entry : envFile.readAll().split('\0')) {
                    if (entry.startsWith("FLATPAK_ID="))
                        return { true, QString::fromUtf8(entry.mid(11)) };
                }
            }
            return { true, {} };
        }
    }

    // 2. exe-path check (Snap, or Flatpak without cgroup visibility)
    const QString exe = QFile::symLinkTarget(
        QStringLiteral("/proc/%1/exe").arg(pid));
    if (exe.contains(QStringLiteral("/flatpak/"))
            || exe.contains(QStringLiteral("/snap/"))) {
        return { true, {} };
    }

    return { false, {} };
}

// ── DbusInterface ─────────────────────────────────────────────────────────────

DbusInterface::DbusInterface(EnvService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
    connect(m_service, &EnvService::userVarsChanged,
            this, &DbusInterface::UserVarsChanged);
    connect(m_service, &EnvService::sessionVarsChanged,
            this, &DbusInterface::SessionVarsChanged);
    connect(m_service, &EnvService::appVarsChanged,
            this, &DbusInterface::AppVarsChanged);
    connect(m_service, &EnvService::systemVarsChanged,
            this, &DbusInterface::SystemVarsChanged);
}

bool DbusInterface::registerOn(QDBusConnection &bus)
{
    if (!bus.registerService(QLatin1String(kServiceName)))
        return false;
    return bus.registerObject(
        QLatin1String(kObjectPath), this,
        QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllSignals
            | QDBusConnection::ExportAllProperties);
}

bool DbusInterface::callerIsSandboxed(QString *outAppId) const
{
    const QString sender = QDBusContext::message().service();
    if (sender.isEmpty())
        return false;

    const QDBusReply<uint> pidReply =
        QDBusContext::connection().interface()->servicePid(sender);
    if (!pidReply.isValid())
        return false;

    const SandboxInfo info = probeProcess(pidReply.value());
    if (outAppId)
        *outAppId = info.appId;
    return info.sandboxed;
}

// ── helpers ───────────────────────────────────────────────────────────────────

QVariantMap DbusInterface::ok()
{
    return { { QStringLiteral("ok"), true }, { QStringLiteral("error"), QString{} } };
}

QVariantMap DbusInterface::err(const QString &message)
{
    return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), message } };
}

// ── User-persistent ───────────────────────────────────────────────────────────
//
// Sandboxed callers are rejected entirely: user-persistent variables are
// session-wide and could interfere with other processes.

QVariantMap DbusInterface::AddUserVar(const QString &key, const QString &value,
                                       const QString &comment, const QString &mergeMode)
{
    if (callerIsSandboxed())
        return err(QStringLiteral("Sandboxed callers may not modify user-persistent variables"));

    return m_service->addUserVar(key, value, comment, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::UpdateUserVar(const QString &key, const QString &value,
                                          const QString &comment, const QString &mergeMode)
{
    if (callerIsSandboxed())
        return err(QStringLiteral("Sandboxed callers may not modify user-persistent variables"));

    return m_service->updateUserVar(key, value, comment, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::DeleteUserVar(const QString &key)
{
    if (callerIsSandboxed())
        return err(QStringLiteral("Sandboxed callers may not modify user-persistent variables"));

    return m_service->deleteUserVar(key)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::SetUserVarEnabled(const QString &key, bool enabled)
{
    if (callerIsSandboxed())
        return err(QStringLiteral("Sandboxed callers may not modify user-persistent variables"));

    return m_service->setUserVarEnabled(key, enabled)
               ? ok()
               : err(m_service->lastError());
}

QVariantList DbusInterface::GetUserVars()
{
    return m_service->userVars();
}

// ── Session ───────────────────────────────────────────────────────────────────
//
// Sandboxed callers may set session variables, but not for critical or high-
// severity keys (PATH, LD_LIBRARY_PATH, WAYLAND_DISPLAY, DISPLAY, etc.) which
// would affect the entire desktop session.

QVariantMap DbusInterface::SetSessionVar(const QString &key, const QString &value,
                                          const QString &mergeMode)
{
    if (callerIsSandboxed()) {
        const QString sev = EnvValidator::validateKey(key).severity;
        if (sev == QLatin1String("critical") || sev == QLatin1String("high")) {
            return err(QStringLiteral("Sandboxed callers may not set session variable %1 "
                                      "(severity: %2)").arg(key, sev));
        }
    }

    return m_service->setSessionVar(key, value, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::UnsetSessionVar(const QString &key)
{
    // Unsetting is always safe — no restriction on sandboxed callers.
    return m_service->unsetSessionVar(key)
               ? ok()
               : err(m_service->lastError());
}

// ── Per-app ───────────────────────────────────────────────────────────────────
//
// Sandboxed callers may only write overrides for their own app ID (the value
// of FLATPAK_ID in their environment).  A sandboxed app with no detectable ID
// is rejected entirely to prevent spoofing another app's environment.

QVariantMap DbusInterface::AddAppVar(const QString &appId, const QString &key,
                                      const QString &value, const QString &mergeMode)
{
    QString callerAppId;
    if (callerIsSandboxed(&callerAppId)) {
        if (callerAppId.isEmpty() || callerAppId != appId) {
            return err(
                QStringLiteral("Sandboxed callers may only set per-app overrides for "
                                "their own app ID (caller: %1, requested: %2)")
                    .arg(callerAppId.isEmpty() ? QStringLiteral("<unknown>") : callerAppId,
                         appId));
        }
    }

    return m_service->addAppVar(appId, key, value, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::RemoveAppVar(const QString &appId, const QString &key)
{
    QString callerAppId;
    if (callerIsSandboxed(&callerAppId)) {
        if (callerAppId.isEmpty() || callerAppId != appId) {
            return err(
                QStringLiteral("Sandboxed callers may only remove per-app overrides for "
                                "their own app ID (caller: %1, requested: %2)")
                    .arg(callerAppId.isEmpty() ? QStringLiteral("<unknown>") : callerAppId,
                         appId));
        }
    }

    return m_service->removeAppVar(appId, key)
               ? ok()
               : err(m_service->lastError());
}

// ── Resolver ─────────────────────────────────────────────────────────────────

QVariantMap DbusInterface::ResolveEnv(const QString &appId)
{
    return m_service->resolveEnv(appId);
}

QStringList DbusInterface::ResolveEnvList(const QString &appId)
{
    return m_service->resolveEnvList(appId);
}

// ── Per-app discovery ─────────────────────────────────────────────────────────

QVariantList DbusInterface::GetAppVars(const QString &appId)
{
    return m_service->appVars(appId);
}

QStringList DbusInterface::ListAppsWithOverrides()
{
    return m_service->appsWithOverrides();
}

// ── System scope ─────────────────────────────────────────────────────────────
//
// Sandboxed callers are always rejected: system-scope writes are privileged
// operations that require polkit and affect the entire machine.

QVariantMap DbusInterface::WriteSystemVar(const QString &key, const QString &value,
                                           const QString &comment,
                                           const QString &mergeMode, bool enabled)
{
    if (callerIsSandboxed())
        return err(QStringLiteral("Sandboxed callers may not write system-scope variables"));

    return m_service->writeSystemVar(key, value, comment, mergeMode, enabled)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::DeleteSystemVar(const QString &key)
{
    if (callerIsSandboxed())
        return err(QStringLiteral("Sandboxed callers may not delete system-scope variables"));

    return m_service->deleteSystemVar(key)
               ? ok()
               : err(m_service->lastError());
}

QVariantList DbusInterface::GetSystemVars()
{
    return m_service->systemVars();
}

// ── Introspection ─────────────────────────────────────────────────────────────

QString DbusInterface::ServiceVersion()
{
    return QStringLiteral("1.0");
}
