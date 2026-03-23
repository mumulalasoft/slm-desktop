#include "PortalSessionManager.h"

#include <QDateTime>
#include <QRegularExpression>

namespace Slm::PortalAdapter {

PortalSessionManager::PortalSessionManager(QObject *parent)
    : QObject(parent)
{
}

void PortalSessionManager::setBus(const QDBusConnection &bus)
{
    m_bus = bus;
}

QString PortalSessionManager::createSession(const Slm::Permissions::CallerIdentity &caller,
                                            Slm::Permissions::Capability capability,
                                            bool revocable,
                                            const QString &requestedPath,
                                            PortalSessionObject **outObject)
{
    const QString objectPath = requestedPath.trimmed().isEmpty()
                                   ? buildObjectPath(caller.appId, nextSessionId())
                                   : requestedPath.trimmed();
    const QString sessionId = objectPath.section(u'/', -1);

    auto *obj = new PortalSessionObject(sessionId, objectPath, caller, capability, revocable, this);
    connect(obj, &PortalSessionObject::sessionFinished, this, &PortalSessionManager::removeSession);

    if (m_bus.isConnected()) {
        m_bus.registerObject(objectPath,
                             obj,
                             QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
                                 | QDBusConnection::ExportAllProperties);
    }
    m_sessions.insert(objectPath, obj);
    emit sessionCreated(objectPath);
    if (outObject) {
        *outObject = obj;
    }
    return objectPath;
}

PortalSessionObject *PortalSessionManager::session(const QString &objectPath) const
{
    return m_sessions.value(objectPath);
}

void PortalSessionManager::removeSession(const QString &objectPath)
{
    PortalSessionObject *obj = m_sessions.take(objectPath);
    if (!obj) {
        return;
    }
    if (m_bus.isConnected()) {
        m_bus.unregisterObject(objectPath);
    }
    obj->deleteLater();
    emit sessionRemoved(objectPath);
}

bool PortalSessionManager::closeSession(const QString &objectPath)
{
    PortalSessionObject *obj = m_sessions.value(objectPath);
    if (!obj) {
        return false;
    }
    obj->Close();
    return true;
}

QString PortalSessionManager::buildObjectPath(const QString &appId, const QString &sessionId) const
{
    return QStringLiteral("/org/desktop/portal/session/%1/%2")
        .arg(sanitizePathComponent(appId.isEmpty() ? QStringLiteral("unknown") : appId),
             sanitizePathComponent(sessionId));
}

QString PortalSessionManager::nextSessionId() const
{
    const quint64 seq = ++const_cast<PortalSessionManager *>(this)->m_sequence;
    return QStringLiteral("s%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(seq);
}

QString PortalSessionManager::sanitizePathComponent(const QString &value)
{
    QString out = value.trimmed();
    if (out.isEmpty()) {
        out = QStringLiteral("unknown");
    }
    out.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));
    return out;
}

} // namespace Slm::PortalAdapter
