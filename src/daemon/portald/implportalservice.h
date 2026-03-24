#pragma once

#include <QDBusUnixFileDescriptor>

#include "../../core/permissions/DBusSecurityGuard.h"
#include "../../core/permissions/PermissionBroker.h"
#include "../../core/permissions/TrustResolver.h"
#include "../../core/permissions/PermissionStore.h"
#include "../../core/permissions/PolicyEngine.h"
#include "../../core/permissions/AuditLogger.h"

#include <QObject>
#include <QDBusContext>
#include <QVariantMap>
#include <QStringList>
#include <QHash>
#include <QSet>

class PortalManager;
namespace Slm::PortalAdapter {
class PortalBackendService;
}

class ImplPortalService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.desktop.slm")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit ImplPortalService(PortalManager *manager,
                               Slm::PortalAdapter::PortalBackendService *backend = nullptr,
                               QObject *parent = nullptr);
    ~ImplPortalService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetBackendInfo() const;
    QVariantMap GetScreencastState() const;
    QVariantMap CloseScreencastSession(const QString &sessionHandle);
    QVariantMap RevokeScreencastSession(const QString &sessionHandle, const QString &reason);
    QVariantMap CloseAllScreencastSessions();

    // Versioned impl API surface (bridge layer, backend stays shared).
    QVariantMap FileChooser(const QString &handle,
                            const QString &appId,
                            const QString &parentWindow,
                            const QVariantMap &options);
    QVariantMap OpenURI(const QString &handle,
                        const QString &appId,
                        const QString &parentWindow,
                        const QString &uri,
                        const QVariantMap &options);
    QVariantMap Screenshot(const QString &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QVariantMap &options);
    QVariantMap ScreenCast(const QString &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QVariantMap &options);

    // Internal bridge helpers used by per-interface DBus adaptors.
    QVariantMap BridgeFileChooser(const QString &handle,
                                  const QString &appId,
                                  const QString &parentWindow,
                                  const QVariantMap &options);
    QVariantMap BridgeOpenURI(const QString &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QString &uri,
                              const QVariantMap &options);
    QVariantMap BridgeScreenshot(const QString &handle,
                                 const QString &appId,
                                 const QString &parentWindow,
                                 const QVariantMap &options);
    QVariantMap BridgeScreenCastCreateSession(const QString &handle,
                                              const QString &appId,
                                              const QString &parentWindow,
                                              const QVariantMap &options);
    QVariantMap BridgeScreenCastSelectSources(const QString &handle,
                                              const QString &appId,
                                              const QString &parentWindow,
                                              const QString &sessionPath,
                                              const QVariantMap &options);
    QVariantMap BridgeScreenCastStart(const QString &handle,
                                      const QString &appId,
                                      const QString &parentWindow,
                                      const QString &sessionPath,
                                      const QVariantMap &options);
    QVariantMap BridgeScreenCastStop(const QString &handle,
                                     const QString &appId,
                                     const QString &parentWindow,
                                     const QString &sessionPath,
                                     const QVariantMap &options);
    QVariantMap BridgeGlobalShortcutsCreateSession(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QVariantMap &options);
    QVariantMap BridgeGlobalShortcutsBindShortcuts(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QString &sessionPath,
                                                   const QVariantMap &options);
    QVariantMap BridgeGlobalShortcutsListBindings(const QString &handle,
                                                  const QString &appId,
                                                  const QString &parentWindow,
                                                  const QString &sessionPath,
                                                  const QVariantMap &options);
    QVariantMap BridgeGlobalShortcutsActivate(const QString &handle,
                                              const QString &appId,
                                              const QString &parentWindow,
                                              const QString &sessionPath,
                                              const QVariantMap &options);
    QVariantMap BridgeGlobalShortcutsDeactivate(const QString &handle,
                                                const QString &appId,
                                                const QString &parentWindow,
                                                const QString &sessionPath,
                                                const QVariantMap &options);
    QVariantMap BridgeInputCaptureCreateSession(const QString &handle,
                                                const QString &appId,
                                                const QString &parentWindow,
                                                const QVariantMap &options);
    QVariantMap BridgeInputCaptureSetPointerBarriers(const QString &handle,
                                                     const QString &appId,
                                                     const QString &parentWindow,
                                                     const QString &sessionPath,
                                                     const QVariantMap &options);
    QVariantMap BridgeInputCaptureEnable(const QString &handle,
                                         const QString &appId,
                                         const QString &parentWindow,
                                         const QString &sessionPath,
                                         const QVariantMap &options);
    QVariantMap BridgeInputCaptureDisable(const QString &handle,
                                          const QString &appId,
                                          const QString &parentWindow,
                                          const QString &sessionPath,
                                          const QVariantMap &options);
    QVariantMap BridgeInputCaptureRelease(const QString &handle,
                                          const QString &appId,
                                          const QString &parentWindow,
                                          const QString &sessionPath,
                                          const QVariantMap &options);
    QVariantMap BridgeSettingsReadAll(const QStringList &namespaces) const;
    QVariant BridgeSettingsRead(const QString &settingNamespace, const QString &key) const;
    QVariantMap BridgeAddNotification(const QString &appId,
                                      const QString &id,
                                      const QVariantMap &notification);
    QVariantMap BridgeRemoveNotification(const QString &appId, const QString &id);
    QVariantMap BridgeInhibit(const QString &handle,
                              const QString &appId,
                              const QString &window,
                              uint flags,
                              const QVariantMap &options);
    QVariantMap BridgeOpenWithQueryHandlers(const QString &handle,
                                            const QString &appId,
                                            const QString &parentWindow,
                                            const QString &path,
                                            const QVariantMap &options);
    QVariantMap BridgeOpenWithOpenFile(const QString &handle,
                                       const QString &appId,
                                       const QString &parentWindow,
                                       const QString &path,
                                       const QString &handlerId,
                                       const QVariantMap &options);
    QVariantMap BridgeOpenWithOpenUri(const QString &handle,
                                      const QString &appId,
                                      const QString &parentWindow,
                                      const QString &uri,
                                      const QString &handlerId,
                                      const QVariantMap &options);
    QVariantMap BridgeDocumentsAdd(const QString &handle,
                                   const QString &appId,
                                   const QString &parentWindow,
                                   const QStringList &uris,
                                   const QVariantMap &options);
    QVariantMap BridgeDocumentsResolve(const QString &handle,
                                       const QString &appId,
                                       const QString &parentWindow,
                                       const QString &token,
                                       const QVariantMap &options);
    QVariantMap BridgeDocumentsList(const QString &handle,
                                    const QString &appId,
                                    const QString &parentWindow,
                                    const QVariantMap &options);
    QVariantMap BridgeDocumentsRemove(const QString &handle,
                                      const QString &appId,
                                      const QString &parentWindow,
                                      const QString &token,
                                      const QVariantMap &options);
    QVariantMap BridgeTrashFile(const QString &handle,
                                const QString &appId,
                                const QString &parentWindow,
                                const QString &path,
                                const QVariantMap &options);
    QVariantMap BridgeTrashEmpty(const QString &handle,
                                 const QString &appId,
                                 const QString &parentWindow,
                                 const QVariantMap &options);

    // Print portal bridge methods.
    // PreparePrint records the settings from the calling app and returns a token.
    // Print receives the document FD and submits the job to the print scheduler.
    QVariantMap BridgePrintPreparePrint(const QString &handle,
                                        const QString &appId,
                                        const QString &parentWindow,
                                        const QString &title,
                                        const QVariantMap &settings,
                                        const QVariantMap &pageSetup,
                                        const QVariantMap &options);
    QVariantMap BridgePrintPrint(const QString &handle,
                                 const QString &appId,
                                 const QString &parentWindow,
                                 const QString &title,
                                 const QDBusUnixFileDescriptor &fd,
                                 const QVariantMap &options);

signals:
    void serviceRegisteredChanged();
    void ScreencastSessionStateChanged(const QString &sessionHandle,
                                       const QString &appId,
                                       bool active,
                                       int activeCount);
    void ScreencastSessionRevoked(const QString &sessionHandle,
                                  const QString &appId,
                                  const QString &reason,
                                  int activeCount);

private:
    QVariantMap enrichCallContext(const QString &handle,
                                  const QString &appId,
                                  const QString &parentWindow,
                                  const QVariantMap &options) const;
    bool canAttemptInputCapturePreempt(const QVariantMap &ctx,
                                       const QVariantMap &options) const;
    void registerDbusService();

    PortalManager *m_manager = nullptr;
    QObject *m_backend = nullptr;
    bool m_serviceRegistered = false;
    QHash<QString, QString> m_screencastSessionOwners;
    QSet<QString> m_activeScreencastSessions;
    QHash<QString, QString> m_globalShortcutSessionOwners;
    QHash<QString, QVariantList> m_globalShortcutSessionBindings;
    QHash<QString, QString> m_globalShortcutAcceleratorOwners;
    QHash<QString, QString> m_inputCaptureSessionOwners;
    Slm::Permissions::PermissionStore m_store;
    Slm::Permissions::PolicyEngine m_engine;
    Slm::Permissions::AuditLogger m_audit;
    Slm::Permissions::TrustResolver m_resolver;
    Slm::Permissions::PermissionBroker m_broker;
    Slm::Permissions::DBusSecurityGuard m_guard;

    // Print settings keyed by request handle, stored during PreparePrint and
    // consumed by Print. Entries are removed after Print or on timeout.
    QHash<QString, QVariantMap> m_printSettings;
};
