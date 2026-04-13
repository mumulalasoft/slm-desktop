#pragma once

#include "recoverydbusadaptor.h"
#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class RecoveryService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Recovery")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit RecoveryService(QObject *parent = nullptr);
    ~RecoveryService() override;

    bool start(QString *error = nullptr);
    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetStatus() const;
    QVariantMap ReportHealth(const QVariantMap &probe);
    QVariantMap ReportCrash(const QString &component, const QString &reason);
    QVariantMap TriggerAutoRecovery(const QString &reason);
    QVariantMap RequestSafeMode(const QString &reason);
    QVariantMap RequestRecoveryPartition(const QString &reason);
    QVariantMap ClearRecoveryFlags();

signals:
    void serviceRegisteredChanged();
    void RecoveryStateChanged(const QVariantMap &state);
    void SafeModeRequested(const QString &reason);
    void RecoveryPartitionRequested(const QString &reason);

private:
    struct ProbeState {
        bool ok = true;
        QString detail;
        QDateTime updatedAt;
    };

    static QString normalizeComponent(const QString &value);
    static bool isCriticalProbe(const QString &component);
    void pruneCrashHistory(const QString &component, const QDateTime &nowUtc);
    int crashCountInWindow(const QString &component) const;
    bool setSafeModeForced(bool enabled, const QString &reason, QString *error = nullptr) const;
    bool writeRecoveryPartitionRequest(const QString &reason, QString *error = nullptr) const;
    bool triggerBootloaderRecovery(const QString &reason, QString *error = nullptr) const;
    QVariantMap composeStatus(const QString &action = QString()) const;

    bool m_serviceRegistered = false;
    int m_autoRecoveryAttempts = 0;
    bool m_recoveryPartitionRequested = false;
    QString m_lastReason;

    QHash<QString, ProbeState> m_probeStates;
    QHash<QString, QList<QDateTime>> m_crashHistoryByComponent;

    Slm::Login::ConfigManager m_config;
};
