#include "firewallserviceclient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Firewall";
constexpr const char kPath[] = "/org/slm/Desktop/Firewall";
constexpr const char kInterface[] = "org.slm.Desktop.Firewall";

QString normalizeMode(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == QLatin1String("public") || mode == QLatin1String("custom")) {
        return mode;
    }
    return QStringLiteral("home");
}

QString normalizePolicy(const QString &value, const QString &fallback)
{
    const QString policy = value.trimmed().toLower();
    if (policy == QLatin1String("allow")
            || policy == QLatin1String("deny")
            || policy == QLatin1String("prompt")) {
        return policy;
    }
    return fallback;
}
}

FirewallServiceClient::FirewallServiceClient(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("/org/freedesktop/DBus"),
                QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("NameOwnerChanged"),
                this,
                SLOT(onNameOwnerChanged(QString,QString,QString)));

    ensureIface();
    refresh();
}

bool FirewallServiceClient::available() const
{
    return m_available;
}

bool FirewallServiceClient::enabled() const
{
    return m_enabled;
}

QString FirewallServiceClient::mode() const
{
    return m_mode;
}

QString FirewallServiceClient::defaultIncomingPolicy() const
{
    return m_defaultIncomingPolicy;
}

QString FirewallServiceClient::defaultOutgoingPolicy() const
{
    return m_defaultOutgoingPolicy;
}

QVariantList FirewallServiceClient::appPolicies() const
{
    return m_appPolicies;
}

QVariantList FirewallServiceClient::ipPolicies() const
{
    return m_ipPolicies;
}

QVariantList FirewallServiceClient::activeConnections() const
{
    return m_activeConnections;
}

bool FirewallServiceClient::refresh()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetStatus"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    if (!payload.value(QStringLiteral("ok"), false).toBool()) {
        return false;
    }
    const bool stateOk = applyStateMap(payload);
    refreshAppPolicies();
    refreshIpPolicies();
    refreshConnections();
    return stateOk;
}

bool FirewallServiceClient::setEnabled(bool enabled)
{
    return callBoolMapMethod(QStringLiteral("SetEnabled"), enabled);
}

bool FirewallServiceClient::setMode(const QString &mode)
{
    return callBoolMapMethod(QStringLiteral("SetMode"), normalizeMode(mode));
}

bool FirewallServiceClient::setDefaultIncomingPolicy(const QString &policy)
{
    return callBoolMapMethod(QStringLiteral("SetDefaultIncomingPolicy"),
                             normalizePolicy(policy, m_defaultIncomingPolicy));
}

bool FirewallServiceClient::setDefaultOutgoingPolicy(const QString &policy)
{
    return callBoolMapMethod(QStringLiteral("SetDefaultOutgoingPolicy"),
                             normalizePolicy(policy, m_defaultOutgoingPolicy));
}

bool FirewallServiceClient::setIpPolicy(const QVariantMap &policy)
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetIpPolicy"), policy);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshIpPolicies();
    }
    return ok;
}

bool FirewallServiceClient::setAppPolicy(const QVariantMap &policy)
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetAppPolicy"), policy);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshAppPolicies();
    }
    return ok;
}

bool FirewallServiceClient::refreshAppPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListAppPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantList next = reply.value();
    if (m_appPolicies != next) {
        m_appPolicies = next;
        emit appPoliciesChanged();
    }
    return true;
}

bool FirewallServiceClient::clearAppPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ClearAppPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok && !m_appPolicies.isEmpty()) {
        m_appPolicies.clear();
        emit appPoliciesChanged();
    }
    return ok;
}

bool FirewallServiceClient::removeAppPolicy(const QString &policyId)
{
    if (!ensureIface()) {
        return false;
    }
    const QString id = policyId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("RemoveAppPolicy"), id);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshAppPolicies();
    }
    return ok;
}

bool FirewallServiceClient::refreshIpPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListIpPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantList next = reply.value();
    if (m_ipPolicies != next) {
        m_ipPolicies = next;
        emit ipPoliciesChanged();
    }
    return true;
}

bool FirewallServiceClient::clearIpPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ClearIpPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        if (!m_ipPolicies.isEmpty()) {
            m_ipPolicies.clear();
            emit ipPoliciesChanged();
        }
    }
    return ok;
}

bool FirewallServiceClient::removeIpPolicy(const QString &policyId)
{
    if (!ensureIface()) {
        return false;
    }
    const QString id = policyId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("RemoveIpPolicy"), id);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshIpPolicies();
    }
    return ok;
}

bool FirewallServiceClient::refreshConnections()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListConnections"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantList next = reply.value();
    if (m_activeConnections != next) {
        m_activeConnections = next;
        emit activeConnectionsChanged();
    }
    return true;
}

void FirewallServiceClient::onNameOwnerChanged(const QString &name,
                                               const QString &oldOwner,
                                               const QString &newOwner)
{
    if (name != QLatin1String(kService)) {
        return;
    }
    Q_UNUSED(oldOwner)
    if (!newOwner.isEmpty()) {
        ensureIface();
        refresh();
        return;
    }

    if (m_available) {
        m_available = false;
        emit availableChanged();
    }
    if (!m_appPolicies.isEmpty()) {
        m_appPolicies.clear();
        emit appPoliciesChanged();
    }
    if (!m_ipPolicies.isEmpty()) {
        m_ipPolicies.clear();
        emit ipPoliciesChanged();
    }
    if (!m_activeConnections.isEmpty()) {
        m_activeConnections.clear();
        emit activeConnectionsChanged();
    }
    delete m_iface;
    m_iface = nullptr;
}

void FirewallServiceClient::onFirewallStateChanged(const QVariantMap &state)
{
    applyStateMap(state);
    refreshAppPolicies();
    refreshIpPolicies();
    refreshConnections();
}

bool FirewallServiceClient::ensureIface()
{
    if (m_iface && m_iface->isValid()) {
        return true;
    }

    delete m_iface;
    m_iface = new QDBusInterface(QLatin1String(kService),
                                 QLatin1String(kPath),
                                 QLatin1String(kInterface),
                                 QDBusConnection::sessionBus(),
                                 this);
    const bool nowAvailable = m_iface->isValid();
    if (nowAvailable != m_available) {
        m_available = nowAvailable;
        emit availableChanged();
    }
    if (nowAvailable) {
        QDBusConnection::sessionBus().connect(QLatin1String(kService),
                                              QLatin1String(kPath),
                                              QLatin1String(kInterface),
                                              QStringLiteral("FirewallStateChanged"),
                                              this,
                                              SLOT(onFirewallStateChanged(QVariantMap)));
    }
    return nowAvailable;
}

bool FirewallServiceClient::callBoolMapMethod(const QString &method, const QVariant &arg)
{
    if (!ensureIface()) {
        return false;
    }

    QDBusReply<QVariantMap> reply = arg.isValid()
            ? m_iface->call(method, arg)
            : m_iface->call(method);
    if (!reply.isValid()) {
        return false;
    }

    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        applyStateMap(payload);
    }
    return ok;
}

bool FirewallServiceClient::applyStateMap(const QVariantMap &map)
{
    const bool nextEnabled = map.value(QStringLiteral("enabled"), m_enabled).toBool();
    const QString nextMode = normalizeMode(map.value(QStringLiteral("mode"), m_mode).toString());
    const QString nextIncoming = normalizePolicy(
        map.value(QStringLiteral("defaultIncomingPolicy"), m_defaultIncomingPolicy).toString(),
        m_defaultIncomingPolicy);
    const QString nextOutgoing = normalizePolicy(
        map.value(QStringLiteral("defaultOutgoingPolicy"), m_defaultOutgoingPolicy).toString(),
        m_defaultOutgoingPolicy);

    const bool changed = (m_enabled != nextEnabled)
            || (m_mode != nextMode)
            || (m_defaultIncomingPolicy != nextIncoming)
            || (m_defaultOutgoingPolicy != nextOutgoing);

    m_enabled = nextEnabled;
    m_mode = nextMode;
    m_defaultIncomingPolicy = nextIncoming;
    m_defaultOutgoingPolicy = nextOutgoing;

    if (changed) {
        emit stateChanged();
    }
    return true;
}
