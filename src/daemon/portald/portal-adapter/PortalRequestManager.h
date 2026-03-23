#pragma once

#include "PortalRequestObject.h"

#include "../../../core/permissions/AccessContext.h"
#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/Capability.h"

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace Slm::PortalAdapter {

class PortalRequestManager : public QObject
{
    Q_OBJECT

public:
    explicit PortalRequestManager(QObject *parent = nullptr);

    void setBus(const QDBusConnection &bus);
    QString createRequest(const Slm::Permissions::CallerIdentity &caller,
                          Slm::Permissions::Capability capability,
                          const Slm::Permissions::AccessContext &context,
                          PortalRequestObject **outObject = nullptr);
    PortalRequestObject *request(const QString &objectPath) const;
    void removeRequest(const QString &objectPath);

signals:
    void requestCreated(const QString &requestPath);
    void requestRemoved(const QString &requestPath);

private:
    QString buildObjectPath(const QString &appId, const QString &requestId) const;
    QString nextRequestId() const;
    static QString sanitizePathComponent(const QString &value);

    QDBusConnection m_bus = QDBusConnection::sessionBus();
    QHash<QString, PortalRequestObject *> m_requests;
    quint64 m_sequence = 0;
};

} // namespace Slm::PortalAdapter
