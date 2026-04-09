#pragma once

#include "src/daemon/firewalld/firewalltypes.h"
#include "src/services/firewall/appidentityclient.h"
#include "src/services/firewall/nftablesadapter.h"
#include "src/services/firewall/policyengine.h"
#include "src/services/firewall/policystore.h"

#include <QDBusContext>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class FirewallService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Firewall")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit FirewallService(QObject *parent = nullptr);
    ~FirewallService() override;

    bool start(QString *error = nullptr);
    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    QVariantMap GetStatus() const;
    QVariantMap SetEnabled(bool enabled);
    QVariantMap SetMode(const QString &mode);
    QVariantMap SetDefaultIncomingPolicy(const QString &policy);
    QVariantMap SetDefaultOutgoingPolicy(const QString &policy);
    QVariantMap EvaluateConnection(const QVariantMap &request);
    QVariantMap SetAppPolicy(const QVariantMap &policy);
    QVariantMap SetIpPolicy(const QVariantMap &policy);
    QVariantList ListConnections() const;

signals:
    void serviceRegisteredChanged();
    void FirewallStateChanged(const QVariantMap &state);
    void PolicyChanged(const QVariantMap &change);
    void ConnectionObserved(const QVariantMap &event);

private:
    bool loadSettingsState();
    bool setSettingsValue(const QString &path, const QVariant &value, QString *error = nullptr) const;

    bool m_serviceRegistered = false;
    Slm::Firewall::FirewallMode m_mode = Slm::Firewall::FirewallMode::Home;
    bool m_enabled = true;
    QString m_defaultIncomingPolicy = QStringLiteral("deny");
    QString m_defaultOutgoingPolicy = QStringLiteral("allow");
    Slm::Firewall::PolicyStore m_store;
    Slm::Firewall::NftablesAdapter m_nftAdapter;
    Slm::Firewall::AppIdentityClient m_identityClient;
    Slm::Firewall::PolicyEngine m_policyEngine;
};
