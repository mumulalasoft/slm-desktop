#include "logdbusinterface.h"

#include <QDBusConnection>

static constexpr char kServiceName[] = "org.slm.Logger1";
static constexpr char kObjectPath[]  = "/org/slm/Logger";

LogDbusInterface::LogDbusInterface(LogService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
    connect(m_service, &LogService::logEntryForSubscription,
            this, &LogDbusInterface::LogEntry);
}

bool LogDbusInterface::registerOn(QDBusConnection &bus)
{
    if (!bus.registerService(QLatin1String(kServiceName)))
        return false;
    return bus.registerObject(
        QLatin1String(kObjectPath), this,
        QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllSignals
            | QDBusConnection::ExportAllProperties);
}

QVariantList LogDbusInterface::GetLogs(const QVariantMap &filter)
{
    return m_service->getLogs(filter);
}

QStringList LogDbusInterface::GetSources()
{
    return m_service->getSources();
}

quint32 LogDbusInterface::Subscribe(const QVariantMap &filter)
{
    const QString source = filter.value(QStringLiteral("source")).toString();
    const QString level  = filter.value(QStringLiteral("level")).toString();
    return m_service->subscribe(source, level);
}

void LogDbusInterface::Unsubscribe(quint32 subscriptionId)
{
    m_service->unsubscribe(subscriptionId);
}

QString LogDbusInterface::ExportBundle(const QVariantMap &filter)
{
    return m_service->exportBundle(filter);
}

void LogDbusInterface::Submit(const QString &source, const QString &level,
                               const QString &message, const QVariantMap &fields)
{
    m_service->submit(source, level, message, fields);
}

QString LogDbusInterface::ServiceVersion()
{
    return QStringLiteral("1.0");
}
