#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::Storage {

class StorageManager : public QObject
{
    Q_OBJECT
public:
    explicit StorageManager(QObject *parent = nullptr);
    ~StorageManager() override;

    void start();
    void stop();
    bool isRunning() const;
    QVariantList queryStorageLocationsSync(int lsblkTimeoutMs = 0) const;
    QVariantMap mountStorageDevice(const QString &devicePath) const;
    QVariantMap unmountStorageDevice(const QString &devicePath) const;
    QVariantMap connectServer(const QString &serverUri) const;
    QVariantMap storagePolicyForPath(const QString &path) const;
    QVariantMap setStoragePolicyForPath(const QString &path,
                                        const QVariantMap &policyPatch,
                                        const QString &scope = QString()) const;
    Q_INVOKABLE void queueStorageChanged();

signals:
    void storageChanged();

private:
    QTimer m_storageChangedDebounceTimer;
    void *m_monitor = nullptr;
    qulonglong m_sigVolumeAdded = 0;
    qulonglong m_sigVolumeRemoved = 0;
    qulonglong m_sigMountAdded = 0;
    qulonglong m_sigMountRemoved = 0;
};

} // namespace Slm::Storage
