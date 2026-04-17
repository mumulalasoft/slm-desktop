#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace Slm::Storage {
class StorageManager;
}

class DevicesManager : public QObject
{
    Q_OBJECT

public:
    explicit DevicesManager(QObject *parent = nullptr);
    ~DevicesManager() override = default;

    Q_INVOKABLE QVariantMap GetStorageLocations() const;
    Q_INVOKABLE QVariantMap StoragePolicyForPath(const QString &path) const;
    Q_INVOKABLE QVariantMap SetStoragePolicyForPath(const QString &path,
                                                    const QVariantMap &policyPatch,
                                                    const QString &scope = QString());
    Q_INVOKABLE QVariantMap Mount(const QString &target);
    Q_INVOKABLE QVariantMap Eject(const QString &target);
    Q_INVOKABLE QVariantMap Unlock(const QString &target, const QString &passphrase);
    Q_INVOKABLE QVariantMap Format(const QString &target, const QString &filesystem);

signals:
    void StorageChanged();

private:
    static QVariantMap makeResult(bool ok, const QString &error = QString());
    QVariantMap runCommand(const QString &program, const QStringList &args, int timeoutMs);

    Slm::Storage::StorageManager *m_storageManager = nullptr;
};
