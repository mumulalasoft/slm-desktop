#pragma once

#include <QObject>
#include <QVariantMap>

class DevicesManager;

class DevicesCompatService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop_shell.Desktop.Devices")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit DevicesCompatService(DevicesManager *manager, QObject *parent = nullptr);
    ~DevicesCompatService() override;

    bool serviceRegistered() const;

public slots:
    QVariantMap Mount(const QString &target);
    QVariantMap Eject(const QString &target);
    QVariantMap Unlock(const QString &target, const QString &passphrase);
    QVariantMap Format(const QString &target, const QString &filesystem);

signals:
    void serviceRegisteredChanged();

private:
    void registerDbusService();

    DevicesManager *m_manager = nullptr;
    bool m_serviceRegistered = false;
};

