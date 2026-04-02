#include "svcmanagerclient.h"

#include <QDBusConnection>
#include <QDBusReply>

static constexpr char kService[]   = "org.slm.ServiceManager1";
static constexpr char kPath[]      = "/org/slm/ServiceManager";
static constexpr char kInterface[] = "org.slm.ServiceManager1";

SvcManagerClient::SvcManagerClient(QObject *parent)
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

bool SvcManagerClient::ensureIface()
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
            QLatin1String(kInterface), QStringLiteral("ComponentStateChanged"),
            this, SIGNAL(componentStateChanged(QString, QString)));
        QDBusConnection::sessionBus().connect(
            QLatin1String(kService), QLatin1String(kPath),
            QLatin1String(kInterface), QStringLiteral("ComponentCrashed"),
            this, SIGNAL(componentCrashed(QString, int)));
    }
    return avail;
}

void SvcManagerClient::onNameOwnerChanged(const QString &name,
                                           const QString &oldOwner,
                                           const QString &newOwner)
{
    if (name != QLatin1String(kService)) return;
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

bool SvcManagerClient::serviceAvailable() const { return m_available; }
QString SvcManagerClient::lastError()      const { return m_lastError; }

bool SvcManagerClient::callOk(const QVariantMap &reply)
{
    m_lastError.clear();
    const bool ok = reply.value(QStringLiteral("ok")).toBool();
    if (!ok)
        m_lastError = reply.value(QStringLiteral("error")).toString();
    return ok;
}

QVariantList SvcManagerClient::listComponents()
{
    if (!ensureIface()) return {};
    QDBusReply<QVariantList> r = m_iface->call(QStringLiteral("ListComponents"));
    return r.isValid() ? r.value() : QVariantList{};
}

QVariantMap SvcManagerClient::getComponent(const QString &name)
{
    if (!ensureIface()) return {};
    QDBusReply<QVariantMap> r = m_iface->call(QStringLiteral("GetComponent"), name);
    return r.isValid() ? r.value() : QVariantMap{};
}

bool SvcManagerClient::restartComponent(const QString &name)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(QStringLiteral("RestartComponent"), name);
    return r.isValid() && callOk(r.value());
}

bool SvcManagerClient::stopComponent(const QString &name)
{
    if (!ensureIface()) return false;
    QDBusReply<QVariantMap> r = m_iface->call(QStringLiteral("StopComponent"), name);
    return r.isValid() && callOk(r.value());
}
