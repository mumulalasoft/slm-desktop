#pragma once

#include <QDBusContext>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

class GlobalMenuService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.GlobalMenu")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString context READ context NOTIFY menusChanged)

public:
    explicit GlobalMenuService(QObject *parent = nullptr);
    ~GlobalMenuService() override;

    bool serviceRegistered() const;
    QString context() const;

public slots:
    QVariantMap Ping() const;
    QVariantList GetTopLevelMenus() const;
    QVariantList GetMenuItems(int menuId) const;
    bool ActivateMenu(int menuId);
    bool ActivateMenuItem(int menuId, int itemId);
    bool SetTopLevelMenus(const QVariantList &menus,
                          const QString &context = QStringLiteral("external"));
    bool ResetToDefault();
    QVariantMap DiagnosticSnapshot() const;

signals:
    void serviceRegisteredChanged();
    void menusChanged();
    void MenuActivated(int menuId, QString label, QString context);
    void MenuItemActivated(int menuId, int itemId, QString label, QString context);

private:
    static QVariantList defaultMenus();
    static QString normalizeLabel(const QString &raw);
    int normalizeMenuId(const QVariantMap &row, int fallbackId) const;
    QString labelForId(int menuId) const;
    QVariantList defaultMenuItemsFor(int menuId) const;
    QString labelForMenuItem(int menuId, int itemId) const;
    bool contextHasToken(const QString &token) const;
    QVariantList contextualMenusForContext() const;
    QVariantList effectiveMenus() const;
    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);

    bool m_serviceRegistered = false;
    QVariantList m_menus;
    QString m_context = QStringLiteral("slm-default");

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
