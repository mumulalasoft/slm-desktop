#include "PortalRequestManager.h"

#include <QDateTime>
#include <QRegularExpression>

namespace Slm::PortalAdapter {

PortalRequestManager::PortalRequestManager(QObject *parent)
    : QObject(parent)
{
}

void PortalRequestManager::setBus(const QDBusConnection &bus)
{
    m_bus = bus;
}

QString PortalRequestManager::createRequest(const Slm::Permissions::CallerIdentity &caller,
                                            Slm::Permissions::Capability capability,
                                            const Slm::Permissions::AccessContext &context,
                                            PortalRequestObject **outObject)
{
    const QString requestId = nextRequestId();
    const QString objectPath = buildObjectPath(caller.appId, requestId);

    auto *obj = new PortalRequestObject(requestId, objectPath, caller, capability, context, this);
    connect(obj, &PortalRequestObject::finished, this, &PortalRequestManager::removeRequest);

    if (m_bus.isConnected()) {
        m_bus.registerObject(objectPath,
                             obj,
                             QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
                                 | QDBusConnection::ExportAllProperties);
    }
    m_requests.insert(objectPath, obj);
    emit requestCreated(objectPath);
    if (outObject) {
        *outObject = obj;
    }
    return objectPath;
}

PortalRequestObject *PortalRequestManager::request(const QString &objectPath) const
{
    return m_requests.value(objectPath);
}

void PortalRequestManager::removeRequest(const QString &objectPath)
{
    PortalRequestObject *obj = m_requests.take(objectPath);
    if (!obj) {
        return;
    }
    if (m_bus.isConnected()) {
        m_bus.unregisterObject(objectPath);
    }
    obj->deleteLater();
    emit requestRemoved(objectPath);
}

QString PortalRequestManager::buildObjectPath(const QString &appId, const QString &requestId) const
{
    return QStringLiteral("/org/desktop/portal/request/%1/%2")
        .arg(sanitizePathComponent(appId.isEmpty() ? QStringLiteral("unknown") : appId),
             sanitizePathComponent(requestId));
}

QString PortalRequestManager::nextRequestId() const
{
    const quint64 seq = ++const_cast<PortalRequestManager *>(this)->m_sequence;
    return QStringLiteral("r%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(seq);
}

QString PortalRequestManager::sanitizePathComponent(const QString &value)
{
    QString out = value.trimmed();
    if (out.isEmpty()) {
        out = QStringLiteral("unknown");
    }
    out.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));
    return out;
}

} // namespace Slm::PortalAdapter
