#include "firewallservice.h"

#include "firewalldbusadaptor.h"

#include <QDBusConnection>

namespace {
constexpr const char kApiVersion[] = "1.0";
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
        {QStringLiteral("apiVersion"), apiVersion()},
    };
}

QVariantMap FirewallService::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    const QVariantMap state = GetStatus();
    emit FirewallStateChanged(state);
    return state;
}

QVariantMap FirewallService::SetMode(const QString &mode)
{
    m_mode = Slm::Firewall::firewallModeFromString(mode);
    const QVariantMap state = GetStatus();
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

QVariantList FirewallService::ListConnections() const
{
    return m_policyEngine.listConnections();
}
