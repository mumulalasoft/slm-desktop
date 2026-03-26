#include "envserviceclient.h"

#include <QDBusConnection>
#include <QDBusReply>

static constexpr char kService[]    = "org.slm.Environment1";
static constexpr char kPath[]       = "/org/slm/Environment";
static constexpr char kInterface[]  = "org.slm.Environment1";

EnvServiceClient::EnvServiceClient(QObject *parent)
    : QObject(parent)
{
    // Watch for the service appearing/disappearing on the session bus.
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(onNameOwnerChanged(QString, QString, QString)));

    // Check if already running.
    ensureIface();
}

bool EnvServiceClient::ensureIface()
{
    if (m_iface && m_iface->isValid())
        return true;

    delete m_iface;
    m_iface = new QDBusInterface(QLatin1String(kService),
                                  QLatin1String(kPath),
                                  QLatin1String(kInterface),
                                  QDBusConnection::sessionBus(),
                                  this);
    const bool avail = m_iface->isValid();
    if (avail != m_available) {
        m_available = avail;
        emit serviceAvailableChanged();
    }

    if (avail) {
        // Wire service signals to our own signals.
        QDBusConnection::sessionBus().connect(
            QLatin1String(kService), QLatin1String(kPath),
            QLatin1String(kInterface), QStringLiteral("UserVarsChanged"),
            this, SIGNAL(userVarsChanged()));
        QDBusConnection::sessionBus().connect(
            QLatin1String(kService), QLatin1String(kPath),
            QLatin1String(kInterface), QStringLiteral("SessionVarsChanged"),
            this, SIGNAL(sessionVarsChanged()));
        QDBusConnection::sessionBus().connect(
            QLatin1String(kService), QLatin1String(kPath),
            QLatin1String(kInterface), QStringLiteral("AppVarsChanged"),
            this, SIGNAL(appVarsChanged(QString)));
        QDBusConnection::sessionBus().connect(
            QLatin1String(kService), QLatin1String(kPath),
            QLatin1String(kInterface), QStringLiteral("SystemVarsChanged"),
            this, SIGNAL(systemVarsChanged()));
    }

    return avail;
}

void EnvServiceClient::onNameOwnerChanged(const QString &name,
                                           const QString &oldOwner,
                                           const QString &newOwner)
{
    if (name != QLatin1String(kService))
        return;
    Q_UNUSED(oldOwner)

    const bool nowAvail = !newOwner.isEmpty();
    if (nowAvail) {
        ensureIface();
    } else {
        m_available = false;
        delete m_iface;
        m_iface = nullptr;
        emit serviceAvailableChanged();
    }
}

bool EnvServiceClient::serviceAvailable() const
{
    return m_available;
}

bool EnvServiceClient::callOk(const QVariantMap &reply)
{
    m_lastError.clear();
    const bool ok = reply.value(QStringLiteral("ok")).toBool();
    if (!ok)
        m_lastError = reply.value(QStringLiteral("error")).toString();
    return ok;
}

// ── User-persistent ───────────────────────────────────────────────────────────

bool EnvServiceClient::addUserVar(const QString &key, const QString &value,
                                   const QString &comment, const QString &mergeMode)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(
        QStringLiteral("AddUserVar"), key, value, comment, mergeMode);
    return r.isValid() && callOk(r.value());
}

bool EnvServiceClient::updateUserVar(const QString &key, const QString &value,
                                      const QString &comment, const QString &mergeMode)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(
        QStringLiteral("UpdateUserVar"), key, value, comment, mergeMode);
    return r.isValid() && callOk(r.value());
}

bool EnvServiceClient::deleteUserVar(const QString &key)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(QStringLiteral("DeleteUserVar"), key);
    return r.isValid() && callOk(r.value());
}

bool EnvServiceClient::setUserVarEnabled(const QString &key, bool enabled)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(
        QStringLiteral("SetUserVarEnabled"), key, enabled);
    return r.isValid() && callOk(r.value());
}

QVariantList EnvServiceClient::getUserVars()
{
    if (!ensureIface()) return {};
    QDBusReply<QVariantList> r = m_iface->call(QStringLiteral("GetUserVars"));
    return r.isValid() ? r.value() : QVariantList{};
}

// ── Session ───────────────────────────────────────────────────────────────────

bool EnvServiceClient::setSessionVar(const QString &key, const QString &value,
                                      const QString &mergeMode)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(
        QStringLiteral("SetSessionVar"), key, value, mergeMode);
    return r.isValid() && callOk(r.value());
}

bool EnvServiceClient::unsetSessionVar(const QString &key)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(QStringLiteral("UnsetSessionVar"), key);
    return r.isValid() && callOk(r.value());
}

// ── Per-app overrides ─────────────────────────────────────────────────────────

bool EnvServiceClient::addAppVar(const QString &appId, const QString &key,
                                  const QString &value, const QString &mergeMode)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(
        QStringLiteral("AddAppVar"), appId, key, value, mergeMode);
    return r.isValid() && callOk(r.value());
}

bool EnvServiceClient::removeAppVar(const QString &appId, const QString &key)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(
        QStringLiteral("RemoveAppVar"), appId, key);
    return r.isValid() && callOk(r.value());
}

QVariantList EnvServiceClient::getAppVars(const QString &appId)
{
    if (!ensureIface()) return {};
    QDBusReply<QVariantList> r = m_iface->call(QStringLiteral("GetAppVars"), appId);
    return r.isValid() ? r.value() : QVariantList{};
}

QStringList EnvServiceClient::listAppsWithOverrides()
{
    if (!ensureIface()) return {};
    QDBusReply<QStringList> r = m_iface->call(QStringLiteral("ListAppsWithOverrides"));
    return r.isValid() ? r.value() : QStringList{};
}

// ── System scope ─────────────────────────────────────────────────────────────

bool EnvServiceClient::writeSystemVar(const QString &key, const QString &value,
                                       const QString &comment, const QString &mergeMode,
                                       bool enabled)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(
        QStringLiteral("WriteSystemVar"), key, value, comment, mergeMode, enabled);
    return r.isValid() && callOk(r.value());
}

bool EnvServiceClient::deleteSystemVar(const QString &key)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(QStringLiteral("DeleteSystemVar"), key);
    return r.isValid() && callOk(r.value());
}

QVariantList EnvServiceClient::getSystemVars()
{
    if (!ensureIface()) return {};
    QDBusReply<QVariantList> r = m_iface->call(QStringLiteral("GetSystemVars"));
    return r.isValid() ? r.value() : QVariantList{};
}

// ── Resolver ─────────────────────────────────────────────────────────────────

QVariantMap EnvServiceClient::resolveEnv(const QString &appId)
{
    if (!ensureIface()) return {};
    QDBusReply<QVariantMap> r = m_iface->call(QStringLiteral("ResolveEnv"), appId);
    return r.isValid() ? r.value() : QVariantMap{};
}

QStringList EnvServiceClient::resolveEnvList(const QString &appId)
{
    if (!ensureIface()) return {};
    QDBusReply<QStringList> r = m_iface->call(QStringLiteral("ResolveEnvList"), appId);
    return r.isValid() ? r.value() : QStringList{};
}

QString EnvServiceClient::lastError() const
{
    return m_lastError;
}
