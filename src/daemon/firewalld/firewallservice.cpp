#include "firewallservice.h"

#include "firewalldbusadaptor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>

namespace {
constexpr const char kApiVersion[] = "1.0";
constexpr const char kSettingsService[] = "org.slm.Desktop.Settings";
constexpr const char kSettingsPath[] = "/org/slm/Desktop/Settings";
constexpr const char kSettingsIface[] = "org.slm.Desktop.Settings";

QString normalizeFirewallPolicy(const QString &value, const QString &fallback)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("allow")
            || normalized == QLatin1String("deny")
            || normalized == QLatin1String("prompt")) {
        return normalized;
    }
    return fallback;
}
}

FirewallService::FirewallService(QObject *parent)
    : QObject(parent)
    , m_policyEngine(&m_store, &m_nftAdapter, &m_identityClient)
{
}

FirewallService::~FirewallService()
{
    if (m_serviceRegistered) {
        Slm::Firewall::Dbus::unregisterServiceObject(QDBusConnection::sessionBus());
    }
}

bool FirewallService::start(QString *error)
{
    if (error) {
        error->clear();
    }

    QString storeError;
    if (!m_store.start(&storeError)) {
        if (error) {
            *error = storeError;
        }
        return false;
    }

    QString nftError;
    if (!m_nftAdapter.ensureBaseRules(&nftError)) {
        if (error) {
            *error = nftError.isEmpty() ? QStringLiteral("failed-ensure-base-rules") : nftError;
        }
        return false;
    }

    loadSettingsState();
    if (!applyCurrentBasePolicy(error)) {
        return false;
    }

    m_serviceRegistered = Slm::Firewall::Dbus::registerServiceObject(QDBusConnection::sessionBus(), this);
    if (!m_serviceRegistered && error) {
        *error = QStringLiteral("dbus-register-failed");
    }
    emit serviceRegisteredChanged();
    return m_serviceRegistered;
}

bool FirewallService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString FirewallService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap FirewallService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), Slm::Firewall::Dbus::serviceName()},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("apiVersion"), apiVersion()},
    };
}

QVariantMap FirewallService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("mode-switch"),
             QStringLiteral("incoming-policy"),
             QStringLiteral("ip-block"),
             QStringLiteral("app-policy"),
             QStringLiteral("connection-observe"),
         }},
    };
}

QVariantMap FirewallService::GetStatus() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("enabled"), m_enabled},
        {QStringLiteral("mode"), Slm::Firewall::firewallModeToString(m_mode)},
        {QStringLiteral("defaultIncomingPolicy"), m_defaultIncomingPolicy},
        {QStringLiteral("defaultOutgoingPolicy"), m_defaultOutgoingPolicy},
        {QStringLiteral("apiVersion"), apiVersion()},
    };
}

QVariantMap FirewallService::SetEnabled(bool enabled)
{
    QString error;
    const bool persisted = setSettingsValue(QStringLiteral("firewall.enabled"), enabled, &error);
    m_enabled = enabled;
    QString packetError;
    const bool packetApplied = applyCurrentBasePolicy(&packetError);
    QVariantMap state = GetStatus();
    state.insert(QStringLiteral("persisted"), persisted);
    state.insert(QStringLiteral("packetApplied"), packetApplied);
    if (!persisted) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("error"), error.isEmpty() ? QStringLiteral("settingsd-unavailable") : error);
    }
    if (!packetApplied) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("packetError"), packetError.isEmpty() ? QStringLiteral("nft-apply-failed") : packetError);
    }
    emit FirewallStateChanged(state);
    return state;
}

QVariantMap FirewallService::SetMode(const QString &mode)
{
    m_mode = Slm::Firewall::firewallModeFromString(mode);
    QString error;
    const bool persisted = setSettingsValue(QStringLiteral("firewall.mode"),
                                            Slm::Firewall::firewallModeToString(m_mode),
                                            &error);
    QString packetError;
    const bool packetApplied = applyCurrentBasePolicy(&packetError);
    QVariantMap state = GetStatus();
    state.insert(QStringLiteral("persisted"), persisted);
    state.insert(QStringLiteral("packetApplied"), packetApplied);
    if (!persisted) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("error"), error.isEmpty() ? QStringLiteral("settingsd-unavailable") : error);
    }
    if (!packetApplied) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("packetError"), packetError.isEmpty() ? QStringLiteral("nft-apply-failed") : packetError);
    }
    emit FirewallStateChanged(state);
    return state;
}

QVariantMap FirewallService::SetDefaultIncomingPolicy(const QString &policy)
{
    m_defaultIncomingPolicy = normalizeFirewallPolicy(policy, m_defaultIncomingPolicy);
    QString error;
    const bool persisted = setSettingsValue(QStringLiteral("firewall.defaultIncomingPolicy"),
                                            m_defaultIncomingPolicy,
                                            &error);
    QString packetError;
    const bool packetApplied = applyCurrentBasePolicy(&packetError);
    QVariantMap state = GetStatus();
    state.insert(QStringLiteral("persisted"), persisted);
    state.insert(QStringLiteral("packetApplied"), packetApplied);
    if (!persisted) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("error"), error.isEmpty() ? QStringLiteral("settingsd-unavailable") : error);
    }
    if (!packetApplied) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("packetError"), packetError.isEmpty() ? QStringLiteral("nft-apply-failed") : packetError);
    }
    emit FirewallStateChanged(state);
    return state;
}

QVariantMap FirewallService::SetDefaultOutgoingPolicy(const QString &policy)
{
    m_defaultOutgoingPolicy = normalizeFirewallPolicy(policy, m_defaultOutgoingPolicy);
    QString error;
    const bool persisted = setSettingsValue(QStringLiteral("firewall.defaultOutgoingPolicy"),
                                            m_defaultOutgoingPolicy,
                                            &error);
    QString packetError;
    const bool packetApplied = applyCurrentBasePolicy(&packetError);
    QVariantMap state = GetStatus();
    state.insert(QStringLiteral("persisted"), persisted);
    state.insert(QStringLiteral("packetApplied"), packetApplied);
    if (!persisted) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("error"), error.isEmpty() ? QStringLiteral("settingsd-unavailable") : error);
    }
    if (!packetApplied) {
        state.insert(QStringLiteral("ok"), false);
        state.insert(QStringLiteral("packetError"), packetError.isEmpty() ? QStringLiteral("nft-apply-failed") : packetError);
    }
    emit FirewallStateChanged(state);
    return state;
}

QVariantMap FirewallService::EvaluateConnection(const QVariantMap &request)
{
    const QVariantMap result = m_policyEngine.evaluateConnection(request);
    emit ConnectionObserved(request);
    return result;
}

QVariantMap FirewallService::SetAppPolicy(const QVariantMap &policy)
{
    const QVariantMap result = m_policyEngine.setAppPolicy(policy);
    emit PolicyChanged(QVariantMap{{QStringLiteral("kind"), QStringLiteral("app")},
                                   {QStringLiteral("policy"), policy},
                                   {QStringLiteral("ok"), result.value(QStringLiteral("ok"), false)}});
    return result;
}

QVariantMap FirewallService::SetIpPolicy(const QVariantMap &policy)
{
    const QVariantMap result = m_policyEngine.setIpPolicy(policy);
    emit PolicyChanged(QVariantMap{{QStringLiteral("kind"), QStringLiteral("ip")},
                                   {QStringLiteral("policy"), policy},
                                   {QStringLiteral("ok"), result.value(QStringLiteral("ok"), false)}});
    return result;
}

QVariantList FirewallService::ListIpPolicies() const
{
    return m_policyEngine.listIpPolicies();
}

QVariantMap FirewallService::ClearIpPolicies()
{
    const QVariantMap result = m_policyEngine.clearIpPolicies();
    emit PolicyChanged(QVariantMap{
        {QStringLiteral("kind"), QStringLiteral("ip")},
        {QStringLiteral("action"), QStringLiteral("clear")},
        {QStringLiteral("ok"), result.value(QStringLiteral("ok"), false)},
    });
    return result;
}

QVariantList FirewallService::ListConnections() const
{
    return m_policyEngine.listConnections();
}

bool FirewallService::loadSettingsState()
{
    QDBusInterface iface(QString::fromLatin1(kSettingsService),
                         QString::fromLatin1(kSettingsPath),
                         QString::fromLatin1(kSettingsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }

    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetSettings"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    if (!payload.value(QStringLiteral("ok"), false).toBool()) {
        return false;
    }

    const QVariantMap settings = payload.value(QStringLiteral("settings")).toMap();
    const QVariantMap firewall = settings.value(QStringLiteral("firewall")).toMap();
    if (!firewall.isEmpty()) {
        m_enabled = firewall.value(QStringLiteral("enabled"), m_enabled).toBool();
        m_mode = Slm::Firewall::firewallModeFromString(
            firewall.value(QStringLiteral("mode"),
                           Slm::Firewall::firewallModeToString(m_mode)).toString());
        m_defaultIncomingPolicy = normalizeFirewallPolicy(
            firewall.value(QStringLiteral("defaultIncomingPolicy"), m_defaultIncomingPolicy).toString(),
            m_defaultIncomingPolicy);
        m_defaultOutgoingPolicy = normalizeFirewallPolicy(
            firewall.value(QStringLiteral("defaultOutgoingPolicy"), m_defaultOutgoingPolicy).toString(),
            m_defaultOutgoingPolicy);
    }
    return true;
}

bool FirewallService::setSettingsValue(const QString &path, const QVariant &value, QString *error) const
{
    if (error) {
        error->clear();
    }
    QDBusInterface iface(QString::fromLatin1(kSettingsService),
                         QString::fromLatin1(kSettingsPath),
                         QString::fromLatin1(kSettingsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (error) {
            *error = QStringLiteral("settingsd-unavailable");
        }
        return false;
    }

    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("SetSetting"),
                                               path,
                                               QVariant::fromValue(QDBusVariant(value)));
    if (!reply.isValid()) {
        if (error) {
            *error = QStringLiteral("settingsd-call-failed");
        }
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (!ok && error) {
        *error = payload.value(QStringLiteral("error")).toString();
    }
    return ok;
}

bool FirewallService::applyCurrentBasePolicy(QString *error)
{
    if (error) {
        error->clear();
    }
    const QVariantMap state{
        {QStringLiteral("enabled"), m_enabled},
        {QStringLiteral("mode"), Slm::Firewall::firewallModeToString(m_mode)},
        {QStringLiteral("defaultIncomingPolicy"), m_defaultIncomingPolicy},
        {QStringLiteral("defaultOutgoingPolicy"), m_defaultOutgoingPolicy},
    };
    const QVariantMap result = m_policyEngine.applyBasePolicy(state);
    const bool ok = result.value(QStringLiteral("ok"), false).toBool();
    if (!ok && error) {
        *error = result.value(QStringLiteral("error")).toString();
    }
    return ok;
}
