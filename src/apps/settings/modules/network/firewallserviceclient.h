#pragma once

#include <QObject>
#include <QString>
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

public:
    explicit FirewallServiceClient(QObject *parent = nullptr);

    bool available() const;
    bool enabled() const;
    QString mode() const;
    QString defaultIncomingPolicy() const;
    QString defaultOutgoingPolicy() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setEnabled(bool enabled);
    Q_INVOKABLE bool setMode(const QString &mode);
    Q_INVOKABLE bool setDefaultIncomingPolicy(const QString &policy);
    Q_INVOKABLE bool setDefaultOutgoingPolicy(const QString &policy);
    Q_INVOKABLE bool setIpPolicy(const QVariantMap &policy);

signals:
    void availableChanged();
    void stateChanged();

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
};
