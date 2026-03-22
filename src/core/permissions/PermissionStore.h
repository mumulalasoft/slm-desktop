#pragma once

#include "Capability.h"
#include "CallerIdentity.h"
#include "PolicyDecision.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

namespace Slm::Permissions {

struct StoredPermission {
    QString appId;
    Capability capability = Capability::Unknown;
    DecisionType decision = DecisionType::Deny;
    QString scope; // session | persistent | resource_specific | global
    QString resourceType;
    QString resourceId;
    QDateTime updatedAt;
    bool valid = false;
};

class PermissionStore {
public:
    PermissionStore();
    ~PermissionStore();

    bool open(const QString &dbPath = QString());
    bool isOpen() const;
    QString databasePath() const;

    bool ensureSchema();
    bool upsertAppRegistry(const CallerIdentity &caller);
    StoredPermission findPermission(const QString &appId,
                                    Capability capability,
                                    const QString &resourceType = QString(),
                                    const QString &resourceId = QString()) const;
    bool savePermission(const QString &appId,
                        Capability capability,
                        DecisionType decision,
                        const QString &scope,
                        const QString &resourceType = QString(),
                        const QString &resourceId = QString());

    bool appendAudit(const QString &appId,
                     Capability capability,
                     DecisionType decision,
                     const QString &resourceType,
                     const QString &reason);

    QVariantList listPermissions() const;
    QVariantList listAuditLogs(int limit = 200) const;
    bool resetPermissionsForApp(const QString &appId);
    bool resetAllPermissions();

private:
    QString defaultPath() const;
    bool ensureConnection();
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_databasePath;
};

} // namespace Slm::Permissions

