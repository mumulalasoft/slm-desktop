#pragma once

#include "../../core/permissions/DBusSecurityGuard.h"
#include "../../core/permissions/PermissionStore.h"
#include "../../core/permissions/PolicyEngine.h"
#include "../../core/permissions/PermissionBroker.h"
#include "../../core/permissions/AuditLogger.h"
#include "../../core/permissions/TrustResolver.h"

#include <QDBusContext>
#include <QDBusMessage>

namespace Slm::Clipboard {

class ClipboardService;

class DBusInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop.Clipboard")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit DBusInterface(ClipboardService *service, QObject *parent = nullptr);
    ~DBusInterface() override;

    bool registerService();
    bool serviceRegistered() const;

public slots:
    QString BackendMode() const;
    QVariantList GetHistory(int limit = 200);
    QVariantList Search(const QString &query, int limit = 200);
    bool PasteItem(qlonglong id);
    bool DeleteItem(qlonglong id);
    bool PinItem(qlonglong id, bool pinned = true);
    bool ClearHistory();

signals:
    void serviceRegisteredChanged();
    void ClipboardChanged(const QVariantMap &item);
    void HistoryUpdated();

private:
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);

    ClipboardService *m_service = nullptr;
    bool m_registered = false;

    Slm::Permissions::PermissionStore m_permissionStore;
    Slm::Permissions::PolicyEngine m_policyEngine;
    Slm::Permissions::PermissionBroker m_permissionBroker;
    Slm::Permissions::AuditLogger m_auditLogger;
    Slm::Permissions::TrustResolver m_trustResolver;
    Slm::Permissions::DBusSecurityGuard m_securityGuard;
};

} // namespace Slm::Clipboard
