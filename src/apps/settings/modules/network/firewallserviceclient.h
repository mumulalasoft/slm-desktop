#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QDBusInterface;

class FirewallServiceClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(QString defaultIncomingPolicy READ defaultIncomingPolicy NOTIFY stateChanged)
    Q_PROPERTY(QString defaultOutgoingPolicy READ defaultOutgoingPolicy NOTIFY stateChanged)
    Q_PROPERTY(int promptCooldownSeconds READ promptCooldownSeconds NOTIFY stateChanged)
    Q_PROPERTY(QVariantList appPolicies READ appPolicies NOTIFY appPoliciesChanged)
    Q_PROPERTY(QVariantList ipPolicies READ ipPolicies NOTIFY ipPoliciesChanged)
    Q_PROPERTY(QVariantList activeConnections READ activeConnections NOTIFY activeConnectionsChanged)

public:
    explicit FirewallServiceClient(QObject *parent = nullptr);

    bool available() const;
    bool enabled() const;
    QString mode() const;
    QString defaultIncomingPolicy() const;
    QString defaultOutgoingPolicy() const;
    int promptCooldownSeconds() const;
    QVariantList appPolicies() const;
    QVariantList ipPolicies() const;
    QVariantList activeConnections() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setEnabled(bool enabled);
    Q_INVOKABLE bool setMode(const QString &mode);
    Q_INVOKABLE bool setDefaultIncomingPolicy(const QString &policy);
    Q_INVOKABLE bool setDefaultOutgoingPolicy(const QString &policy);
    Q_INVOKABLE bool setPromptCooldownSeconds(int seconds);
    Q_INVOKABLE QVariantMap evaluateConnection(const QVariantMap &request);
    Q_INVOKABLE bool resolveConnectionDecision(const QVariantMap &request,
                                               const QString &decision,
                                               bool remember);
    Q_INVOKABLE bool setAppPolicy(const QVariantMap &policy);
    Q_INVOKABLE bool refreshAppPolicies();
    Q_INVOKABLE bool clearAppPolicies();
    Q_INVOKABLE bool removeAppPolicy(const QString &policyId);
    Q_INVOKABLE bool setIpPolicy(const QVariantMap &policy);
    Q_INVOKABLE bool refreshIpPolicies();
    Q_INVOKABLE bool clearIpPolicies();
    Q_INVOKABLE bool removeIpPolicy(const QString &policyId);
    Q_INVOKABLE bool refreshConnections();

signals:
    void availableChanged();
    void stateChanged();
    void appPoliciesChanged();
    void ipPoliciesChanged();
    void activeConnectionsChanged();

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onFirewallStateChanged(const QVariantMap &state);

private:
    bool ensureIface();
    bool callBoolMapMethod(const QString &method, const QVariant &arg);
    bool applyStateMap(const QVariantMap &map);

    QDBusInterface *m_iface = nullptr;
    bool m_available = false;
    bool m_enabled = true;
    QString m_mode = QStringLiteral("home");
    QString m_defaultIncomingPolicy = QStringLiteral("deny");
    QString m_defaultOutgoingPolicy = QStringLiteral("allow");
    int m_promptCooldownSeconds = 20;
    QVariantList m_appPolicies;
    QVariantList m_ipPolicies;
    QVariantList m_activeConnections;
};
