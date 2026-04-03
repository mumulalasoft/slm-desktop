#include "implportaladaptors.h"

#include "implportalservice.h"

ImplPortalFileChooserAdaptor::ImplPortalFileChooserAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalFileChooserAdaptor::OpenFile(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QVariantMap &options)
{
    return m_service ? m_service->BridgeFileChooser(handle, appId, parentWindow, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalFileChooserAdaptor::SaveFile(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QVariantMap &options)
{
    QVariantMap local = options;
    local.insert(QStringLiteral("mode"), QStringLiteral("save"));
    return m_service ? m_service->BridgeFileChooser(handle, appId, parentWindow, local)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalOpenURIAdaptor::ImplPortalOpenURIAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalOpenURIAdaptor::OpenURI(const QString &handle,
                                              const QString &appId,
                                              const QString &parentWindow,
                                              const QString &uri,
                                              const QVariantMap &options)
{
    return m_service ? m_service->BridgeOpenURI(handle, appId, parentWindow, uri, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalScreenshotAdaptor::ImplPortalScreenshotAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalScreenshotAdaptor::Screenshot(const QString &handle,
                                                    const QString &appId,
                                                    const QString &parentWindow,
                                                    const QVariantMap &options)
{
    return m_service ? m_service->BridgeScreenshot(handle, appId, parentWindow, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalScreenCastAdaptor::ImplPortalScreenCastAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalScreenCastAdaptor::CreateSession(const QString &handle,
                                                       const QString &appId,
                                                       const QString &parentWindow,
                                                       const QVariantMap &options)
{
    return m_service ? m_service->BridgeScreenCastCreateSession(handle, appId, parentWindow, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalScreenCastAdaptor::SelectSources(const QString &handle,
                                                       const QString &appId,
                                                       const QString &parentWindow,
                                                       const QString &sessionPath,
                                                       const QVariantMap &options)
{
    return m_service
               ? m_service->BridgeScreenCastSelectSources(handle,
                                                          appId,
                                                          parentWindow,
                                                          sessionPath,
                                                          options)
               : QVariantMap{{QStringLiteral("ok"), false},
                             {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalScreenCastAdaptor::Start(const QString &handle,
                                               const QString &appId,
                                               const QString &parentWindow,
                                               const QString &sessionPath,
                                               const QVariantMap &options)
{
    return m_service ? m_service->BridgeScreenCastStart(handle,
                                                        appId,
                                                        parentWindow,
                                                        sessionPath,
                                                        options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalScreenCastAdaptor::Stop(const QString &handle,
                                              const QString &appId,
                                              const QString &parentWindow,
                                              const QString &sessionPath,
                                              const QVariantMap &options)
{
    return m_service ? m_service->BridgeScreenCastStop(handle,
                                                       appId,
                                                       parentWindow,
                                                       sessionPath,
                                                       options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalGlobalShortcutsAdaptor::ImplPortalGlobalShortcutsAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalGlobalShortcutsAdaptor::CreateSession(const QString &handle,
                                                            const QString &appId,
                                                            const QString &parentWindow,
                                                            const QVariantMap &options)
{
    return m_service ? m_service->BridgeGlobalShortcutsCreateSession(handle, appId, parentWindow, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalGlobalShortcutsAdaptor::BindShortcuts(const QString &handle,
                                                            const QString &appId,
                                                            const QString &parentWindow,
                                                            const QString &sessionPath,
                                                            const QVariantMap &options)
{
    return m_service ? m_service->BridgeGlobalShortcutsBindShortcuts(handle,
                                                                     appId,
                                                                     parentWindow,
                                                                     sessionPath,
                                                                     options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalGlobalShortcutsAdaptor::ListBindings(const QString &handle,
                                                           const QString &appId,
                                                           const QString &parentWindow,
                                                           const QString &sessionPath,
                                                           const QVariantMap &options)
{
    return m_service ? m_service->BridgeGlobalShortcutsListBindings(handle,
                                                                    appId,
                                                                    parentWindow,
                                                                    sessionPath,
                                                                    options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalGlobalShortcutsAdaptor::Activate(const QString &handle,
                                                       const QString &appId,
                                                       const QString &parentWindow,
                                                       const QString &sessionPath,
                                                       const QVariantMap &options)
{
    return m_service ? m_service->BridgeGlobalShortcutsActivate(handle,
                                                                appId,
                                                                parentWindow,
                                                                sessionPath,
                                                                options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalGlobalShortcutsAdaptor::Deactivate(const QString &handle,
                                                         const QString &appId,
                                                         const QString &parentWindow,
                                                         const QString &sessionPath,
                                                         const QVariantMap &options)
{
    return m_service ? m_service->BridgeGlobalShortcutsDeactivate(handle,
                                                                  appId,
                                                                  parentWindow,
                                                                  sessionPath,
                                                                  options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalInputCaptureAdaptor::ImplPortalInputCaptureAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalInputCaptureAdaptor::CreateSession(const QString &handle,
                                                         const QString &appId,
                                                         const QString &parentWindow,
                                                         const QVariantMap &options)
{
    return m_service ? m_service->BridgeInputCaptureCreateSession(handle, appId, parentWindow, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalInputCaptureAdaptor::SetPointerBarriers(const QString &handle,
                                                              const QString &appId,
                                                              const QString &parentWindow,
                                                              const QString &sessionPath,
                                                              const QVariantMap &options)
{
    return m_service ? m_service->BridgeInputCaptureSetPointerBarriers(handle,
                                                                       appId,
                                                                       parentWindow,
                                                                       sessionPath,
                                                                       options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalInputCaptureAdaptor::Enable(const QString &handle,
                                                  const QString &appId,
                                                  const QString &parentWindow,
                                                  const QString &sessionPath,
                                                  const QVariantMap &options)
{
    return m_service ? m_service->BridgeInputCaptureEnable(handle,
                                                           appId,
                                                           parentWindow,
                                                           sessionPath,
                                                           options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalInputCaptureAdaptor::Disable(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QString &sessionPath,
                                                   const QVariantMap &options)
{
    return m_service ? m_service->BridgeInputCaptureDisable(handle,
                                                            appId,
                                                            parentWindow,
                                                            sessionPath,
                                                            options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalInputCaptureAdaptor::Release(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QString &sessionPath,
                                                   const QVariantMap &options)
{
    return m_service ? m_service->BridgeInputCaptureRelease(handle,
                                                            appId,
                                                            parentWindow,
                                                            sessionPath,
                                                            options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalSettingsAdaptor::ImplPortalSettingsAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalSettingsAdaptor::ReadAll(const QStringList &namespaces)
{
    return m_service ? m_service->BridgeSettingsReadAll(namespaces) : QVariantMap{};
}

QDBusVariant ImplPortalSettingsAdaptor::Read(const QString &settingNamespace, const QString &key)
{
    return QDBusVariant(m_service ? m_service->BridgeSettingsRead(settingNamespace, key) : QVariant{});
}

ImplPortalNotificationAdaptor::ImplPortalNotificationAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalNotificationAdaptor::AddNotification(const QString &appId,
                                                           const QString &id,
                                                           const QVariantMap &notification)
{
    return m_service ? m_service->BridgeAddNotification(appId, id, notification)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalNotificationAdaptor::RemoveNotification(const QString &appId, const QString &id)
{
    return m_service ? m_service->BridgeRemoveNotification(appId, id)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalInhibitAdaptor::ImplPortalInhibitAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalInhibitAdaptor::Inhibit(const QString &handle,
                                              const QString &appId,
                                              const QString &window,
                                              uint flags,
                                              const QVariantMap &options)
{
    return m_service ? m_service->BridgeInhibit(handle, appId, window, flags, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalOpenWithAdaptor::ImplPortalOpenWithAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalOpenWithAdaptor::QueryHandlers(const QString &handle,
                                                     const QString &appId,
                                                     const QString &parentWindow,
                                                     const QString &path,
                                                     const QVariantMap &options)
{
    return m_service ? m_service->BridgeOpenWithQueryHandlers(handle,
                                                              appId,
                                                              parentWindow,
                                                              path,
                                                              options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalOpenWithAdaptor::OpenFileWith(const QString &handle,
                                                    const QString &appId,
                                                    const QString &parentWindow,
                                                    const QString &path,
                                                    const QString &handlerId,
                                                    const QVariantMap &options)
{
    return m_service ? m_service->BridgeOpenWithOpenFile(handle,
                                                         appId,
                                                         parentWindow,
                                                         path,
                                                         handlerId,
                                                         options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalOpenWithAdaptor::OpenUriWith(const QString &handle,
                                                   const QString &appId,
                                                   const QString &parentWindow,
                                                   const QString &uri,
                                                   const QString &handlerId,
                                                   const QVariantMap &options)
{
    return m_service ? m_service->BridgeOpenWithOpenUri(handle,
                                                        appId,
                                                        parentWindow,
                                                        uri,
                                                        handlerId,
                                                        options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalDocumentsAdaptor::ImplPortalDocumentsAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalDocumentsAdaptor::Add(const QString &handle,
                                            const QString &appId,
                                            const QString &parentWindow,
                                            const QStringList &uris,
                                            const QVariantMap &options)
{
    return m_service ? m_service->BridgeDocumentsAdd(handle, appId, parentWindow, uris, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalDocumentsAdaptor::Resolve(const QString &handle,
                                                const QString &appId,
                                                const QString &parentWindow,
                                                const QString &token,
                                                const QVariantMap &options)
{
    return m_service ? m_service->BridgeDocumentsResolve(handle,
                                                         appId,
                                                         parentWindow,
                                                         token,
                                                         options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalDocumentsAdaptor::List(const QString &handle,
                                             const QString &appId,
                                             const QString &parentWindow,
                                             const QVariantMap &options)
{
    return m_service ? m_service->BridgeDocumentsList(handle, appId, parentWindow, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalDocumentsAdaptor::Remove(const QString &handle,
                                               const QString &appId,
                                               const QString &parentWindow,
                                               const QString &token,
                                               const QVariantMap &options)
{
    return m_service ? m_service->BridgeDocumentsRemove(handle,
                                                        appId,
                                                        parentWindow,
                                                        token,
                                                        options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

ImplPortalTrashAdaptor::ImplPortalTrashAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalTrashAdaptor::TrashFile(const QString &handle,
                                              const QString &appId,
                                              const QString &parentWindow,
                                              const QString &path,
                                              const QVariantMap &options)
{
    return m_service ? m_service->BridgeTrashFile(handle, appId, parentWindow, path, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalTrashAdaptor::EmptyTrash(const QString &handle,
                                               const QString &appId,
                                               const QString &parentWindow,
                                               const QVariantMap &options)
{
    return m_service ? m_service->BridgeTrashEmpty(handle, appId, parentWindow, options)
                     : QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

// ── Print ──────────────────────────────────────────────────────────────────

ImplPortalPrintAdaptor::ImplPortalPrintAdaptor(ImplPortalService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

QVariantMap ImplPortalPrintAdaptor::PreparePrint(const QString &handle,
                                                  const QString &appId,
                                                  const QString &parentWindow,
                                                  const QString &title,
                                                  const QVariantMap &settings,
                                                  const QVariantMap &pageSetup,
                                                  const QVariantMap &options)
{
    return m_service
        ? m_service->BridgePrintPreparePrint(handle, appId, parentWindow,
                                             title, settings, pageSetup, options)
        : QVariantMap{{QStringLiteral("response"), 2u},
                      {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}

QVariantMap ImplPortalPrintAdaptor::Print(const QString &handle,
                                           const QString &appId,
                                           const QString &parentWindow,
                                           const QString &title,
                                           const QDBusUnixFileDescriptor &fd,
                                           const QVariantMap &options)
{
    return m_service
        ? m_service->BridgePrintPrint(handle, appId, parentWindow, title, fd, options)
        : QVariantMap{{QStringLiteral("response"), 2u},
                      {QStringLiteral("error"), QStringLiteral("BackendUnavailable")}};
}
