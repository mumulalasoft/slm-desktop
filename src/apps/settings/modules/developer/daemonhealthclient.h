#pragma once

#include <QDBusInterface>
#include <QObject>
#include <QVariantMap>

class DaemonHealthClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)

public:
    explicit DaemonHealthClient(QObject *parent = nullptr);

    bool serviceAvailable() const;
    QVariantMap snapshot() const;

    Q_INVOKABLE void refresh();

signals:
    void serviceAvailableChanged();
    void snapshotChanged();

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    bool ensureIface();

    QDBusInterface *m_iface = nullptr;
    bool m_available = false;
    QVariantMap m_snapshot;
};
