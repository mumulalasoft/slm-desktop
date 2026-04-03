#pragma once

#include <QDBusInterface>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// LogServiceClient — thin D-Bus proxy for org.slm.Logger1.
//
// When the service is not running, all calls return empty results and
// serviceAvailable returns false.
//
class LogServiceClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)

public:
    explicit LogServiceClient(QObject *parent = nullptr);

    bool serviceAvailable() const;

    QVariantList getLogs(const QVariantMap &filter = {});
    QStringList  getSources();
    quint32      subscribe(const QString &source = {}, const QString &level = {});
    void         unsubscribe(quint32 subscriptionId);
    QString      exportBundle(const QVariantMap &filter = {});

    QString lastError() const;

signals:
    void serviceAvailableChanged();
    void logEntry(quint32 subscriptionId, const QVariantMap &entry);

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                            const QString &newOwner);
    void onLogEntry(quint32 subscriptionId, const QVariantMap &entry);

private:
    bool ensureIface();

    QDBusInterface *m_iface     = nullptr;
    bool            m_available = false;
    QString         m_lastError;
};
