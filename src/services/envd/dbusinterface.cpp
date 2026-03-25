#include "dbusinterface.h"

#include <QDBusConnection>

static constexpr char kServiceName[] = "org.slm.Environment1";
static constexpr char kObjectPath[]  = "/org/slm/Environment";

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

// ── helpers ───────────────────────────────────────────────────────────────────

QVariantMap DbusInterface::ok()
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("error"), QString{}}};
}

QVariantMap DbusInterface::err(const QString &message)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), message}};
}

// ── User-persistent ───────────────────────────────────────────────────────────

QVariantMap DbusInterface::AddUserVar(const QString &key, const QString &value,
                                       const QString &comment, const QString &mergeMode)
{
    return m_service->addUserVar(key, value, comment, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::UpdateUserVar(const QString &key, const QString &value,
                                          const QString &comment, const QString &mergeMode)
{
    return m_service->updateUserVar(key, value, comment, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::DeleteUserVar(const QString &key)
{
    return m_service->deleteUserVar(key)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::SetUserVarEnabled(const QString &key, bool enabled)
{
    return m_service->setUserVarEnabled(key, enabled)
               ? ok()
               : err(m_service->lastError());
}

QVariantList DbusInterface::GetUserVars()
{
    return m_service->userVars();
}

// ── Session ───────────────────────────────────────────────────────────────────

QVariantMap DbusInterface::SetSessionVar(const QString &key, const QString &value,
                                          const QString &mergeMode)
{
    return m_service->setSessionVar(key, value, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::UnsetSessionVar(const QString &key)
{
    return m_service->unsetSessionVar(key)
               ? ok()
               : err(m_service->lastError());
}

// ── Per-app ───────────────────────────────────────────────────────────────────

QVariantMap DbusInterface::AddAppVar(const QString &appId, const QString &key,
                                      const QString &value, const QString &mergeMode)
{
    return m_service->addAppVar(appId, key, value, mergeMode)
               ? ok()
               : err(m_service->lastError());
}

QVariantMap DbusInterface::RemoveAppVar(const QString &appId, const QString &key)
{
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

// ── Introspection ─────────────────────────────────────────────────────────────

QString DbusInterface::ServiceVersion()
{
    return QStringLiteral("1.0");
}
