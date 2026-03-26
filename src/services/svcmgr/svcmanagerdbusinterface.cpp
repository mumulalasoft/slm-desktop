#include "svcmanagerdbusinterface.h"

#include <QDBusConnection>

static constexpr char kServiceName[] = "org.slm.ServiceManager1";
static constexpr char kObjectPath[]  = "/org/slm/ServiceManager";

SvcManagerDbusInterface::SvcManagerDbusInterface(SvcManagerService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
    connect(m_service, &SvcManagerService::componentStateChanged,
            this, &SvcManagerDbusInterface::ComponentStateChanged);
    connect(m_service, &SvcManagerService::componentCrashed,
            this, &SvcManagerDbusInterface::ComponentCrashed);
}

bool SvcManagerDbusInterface::registerOn(QDBusConnection &bus)
{
    if (!bus.registerService(QLatin1String(kServiceName)))
        return false;
    return bus.registerObject(
        QLatin1String(kObjectPath), this,
        QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllSignals
            | QDBusConnection::ExportAllProperties);
}

QVariantList SvcManagerDbusInterface::ListComponents()
{
    return m_service->listComponents();
}

QVariantMap SvcManagerDbusInterface::GetComponent(const QString &name)
{
    return m_service->getComponent(name);
}

QVariantMap SvcManagerDbusInterface::RestartComponent(const QString &name)
{
    return m_service->restartComponent(name);
}

QVariantMap SvcManagerDbusInterface::StopComponent(const QString &name)
{
    return m_service->stopComponent(name);
}

QString SvcManagerDbusInterface::ServiceVersion()
{
    return QStringLiteral("1.0");
}
