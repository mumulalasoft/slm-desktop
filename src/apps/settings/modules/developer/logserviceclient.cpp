#include "logserviceclient.h"

#include <QDBusConnection>
#include <QDBusReply>

static constexpr char kService[]   = "org.slm.Logger1";
static constexpr char kPath[]      = "/org/slm/Logger";
static constexpr char kInterface[] = "org.slm.Logger1";

LogServiceClient::LogServiceClient(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(onNameOwnerChanged(QString, QString, QString)));

    ensureIface();
}

bool LogServiceClient::ensureIface()
{
    if (m_iface && m_iface->isValid())
        return true;

    delete m_iface;
    m_iface = new QDBusInterface(QLatin1String(kService),
                                  QLatin1String(kPath),
                                  QLatin1String(kInterface),
                                  QDBusConnection::sessionBus(),
                                  this);
    const bool avail = m_iface->isValid();
    if (avail != m_available) {
        m_available = avail;
        emit serviceAvailableChanged();
    }

    if (avail) {
        QDBusConnection::sessionBus().connect(
            QLatin1String(kService), QLatin1String(kPath),
            QLatin1String(kInterface), QStringLiteral("LogEntry"),
            this, SLOT(onLogEntry(quint32, QVariantMap)));
    }

    return avail;
}

void LogServiceClient::onNameOwnerChanged(const QString &name,
                                           const QString &oldOwner,
                                           const QString &newOwner)
{
    if (name != QLatin1String(kService))
        return;
    Q_UNUSED(oldOwner)
    if (!newOwner.isEmpty()) {
        ensureIface();
    } else {
        m_available = false;
        delete m_iface;
        m_iface = nullptr;
        emit serviceAvailableChanged();
    }
}

void LogServiceClient::onLogEntry(quint32 subscriptionId, const QVariantMap &entry)
{
    emit logEntry(subscriptionId, entry);
}

bool LogServiceClient::serviceAvailable() const { return m_available; }
QString LogServiceClient::lastError() const { return m_lastError; }

QVariantList LogServiceClient::getLogs(const QVariantMap &filter)
{
    if (!ensureIface()) return {};
    QDBusReply<QVariantList> r = m_iface->call(QStringLiteral("GetLogs"), filter);
    return r.isValid() ? r.value() : QVariantList{};
}

QStringList LogServiceClient::getSources()
{
    if (!ensureIface()) return {};
    QDBusReply<QStringList> r = m_iface->call(QStringLiteral("GetSources"));
    return r.isValid() ? r.value() : QStringList{};
}

quint32 LogServiceClient::subscribe(const QString &source, const QString &level)
{
    if (!ensureIface()) return 0;
    QVariantMap filter;
    if (!source.isEmpty()) filter[QStringLiteral("source")] = source;
    if (!level.isEmpty())  filter[QStringLiteral("level")]  = level;
    QDBusReply<quint32> r = m_iface->call(QStringLiteral("Subscribe"), filter);
    return r.isValid() ? r.value() : 0;
}

void LogServiceClient::unsubscribe(quint32 subscriptionId)
{
    if (!ensureIface()) return;
    m_iface->call(QStringLiteral("Unsubscribe"), subscriptionId);
}

QString LogServiceClient::exportBundle(const QVariantMap &filter)
{
    if (!ensureIface()) return {};
    QDBusReply<QString> r = m_iface->call(QStringLiteral("ExportBundle"), filter);
    return r.isValid() ? r.value() : QString{};
}
