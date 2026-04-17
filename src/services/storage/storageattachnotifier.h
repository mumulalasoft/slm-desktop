#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class NotificationManager;

class StorageAttachNotifier : public QObject
{
    Q_OBJECT

public:
    explicit StorageAttachNotifier(NotificationManager *notificationManager,
                                   QObject *parent = nullptr);

private slots:
    void onStorageLocationsChanged();
    void onRefreshTimerFired();
    void onNotificationActionInvoked(uint id, const QString &actionKey);
    void onNotificationClosed(uint id, uint reason);

private:
    struct VolumeRow {
        QString device;
        QString path;
        bool mounted = false;
    };

    struct DeviceGroupPayload {
        QString label;
        QVector<VolumeRow> volumes;
    };

    struct DeviceGroupSnapshot {
        int visibleCount = 0;
        QString label;
        QVector<VolumeRow> volumes;
    };

    QVariantList fetchStorageRows() const;
    void refreshSnapshot(bool notifyOnAttach);
    QString deviceGroupKeyFromRow(const QVariantMap &row) const;
    QString deviceGroupLabelFromRow(const QVariantMap &row) const;
    VolumeRow volumeRowFromVariant(const QVariantMap &row) const;
    QString actionTargetForVolume(const VolumeRow &volume) const;
    bool mountTarget(const QString &target, QString *mountedPathOut) const;
    bool ejectTarget(const QString &target) const;
    bool openVolume(const VolumeRow &volume) const;
    void handleOpenAction(const DeviceGroupPayload &payload) const;
    void handleEjectAction(const DeviceGroupPayload &payload) const;

    NotificationManager *m_notificationManager = nullptr;
    QTimer m_refreshDebounceTimer;
    bool m_snapshotReady = false;
    int m_attachCooldownMs = 12000;
    QHash<QString, DeviceGroupSnapshot> m_prevGroups;
    QHash<QString, qint64> m_lastNotifiedMsByGroup;
    QHash<uint, DeviceGroupPayload> m_payloadByNotificationId;
};

