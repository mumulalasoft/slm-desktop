#pragma once

#include <QDBusInterface>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// SvcManagerClient — thin D-Bus proxy for org.slm.ServiceManager1.
//
class SvcManagerClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)

public:
    explicit SvcManagerClient(QObject *parent = nullptr);

    bool serviceAvailable() const;

    QVariantList listComponents();
    QVariantMap  getComponent(const QString &name);
    bool         restartComponent(const QString &name);
    bool         stopComponent(const QString &name);

    QString lastError() const;

signals:
    void serviceAvailableChanged();
    void componentStateChanged(const QString &name, const QString &newStatus);
    void componentCrashed(const QString &name, int exitCode);

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                            const QString &newOwner);

private:
    bool ensureIface();
    bool callOk(const QVariantMap &reply);

    QDBusInterface *m_iface     = nullptr;
    bool            m_available = false;
    QString         m_lastError;
};
