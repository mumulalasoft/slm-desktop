#pragma once

#include "PortalSessionObject.h"

#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/Capability.h"

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace Slm::PortalAdapter {

class PortalSessionManager : public QObject
{
    Q_OBJECT

public:
    explicit PortalSessionManager(QObject *parent = nullptr);

    void setBus(const QDBusConnection &bus);
    QString createSession(const Slm::Permissions::CallerIdentity &caller,
                          Slm::Permissions::Capability capability,
                          bool revocable,
                          const QString &requestedPath = QString(),
                          PortalSessionObject **outObject = nullptr);
    PortalSessionObject *session(const QString &objectPath) const;
    void removeSession(const QString &objectPath);
    bool closeSession(const QString &objectPath);

signals:
    void sessionCreated(const QString &sessionPath);
    void sessionRemoved(const QString &sessionPath);

private:
    QString buildObjectPath(const QString &appId, const QString &sessionId) const;
    QString nextSessionId() const;
    static QString sanitizePathComponent(const QString &value);

    QDBusConnection m_bus = QDBusConnection::sessionBus();
    QHash<QString, PortalSessionObject *> m_sessions;
    quint64 m_sequence = 0;
};

} // namespace Slm::PortalAdapter
