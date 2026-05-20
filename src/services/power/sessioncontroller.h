#pragma once

#include <QObject>
#include <QVariantList>

class SessionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool preparing READ preparing NOTIFY preparingChanged)
    Q_PROPERTY(QVariantList remainingApps READ remainingApps NOTIFY remainingAppsChanged)

public:
    explicit SessionController(QObject *parent = nullptr);

    bool preparing() const;
    QVariantList remainingApps() const;

    Q_INVOKABLE bool preparePowerAction(const QString &action);
    Q_INVOKABLE void terminateRemainingApps(const QString &action = QString());

signals:
    void preparingChanged(bool preparing);
    void remainingAppsChanged(const QVariantList &apps);
    void preparePowerActionFinished(const QString &action, const QVariantList &remainingApps);

private:
    QVariantList collectRemainingApps() const;

    bool m_preparing = false;
    QString m_pendingAction;
    QVariantList m_remainingApps;
};
