#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusVariant>
#include <QVariantMap>

class ImplPortalService;

class ImplPortalFileChooserAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")

public:
    explicit ImplPortalFileChooserAdaptor(ImplPortalService *service);

public slots:
    QVariantMap OpenFile(const QString &handle,
                         const QString &appId,
                         const QString &parentWindow,
                         const QVariantMap &options);
    QVariantMap SaveFile(const QString &handle,
                         const QString &appId,
                         const QString &parentWindow,
                         const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalOpenURIAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.OpenURI")

public:
    explicit ImplPortalOpenURIAdaptor(ImplPortalService *service);

public slots:
    QVariantMap OpenURI(const QString &handle,
                        const QString &appId,
                        const QString &parentWindow,
                        const QString &uri,
                        const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalScreenshotAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Screenshot")

public:
    explicit ImplPortalScreenshotAdaptor(ImplPortalService *service);

public slots:
    QVariantMap Screenshot(const QString &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalScreenCastAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.ScreenCast")

public:
    explicit ImplPortalScreenCastAdaptor(ImplPortalService *service);

public slots:
    QVariantMap CreateSession(const QString &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QVariantMap &options);
    QVariantMap SelectSources(const QString &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QString &sessionPath,
                              const QVariantMap &options);
    QVariantMap Start(const QString &handle,
                      const QString &appId,
                      const QString &parentWindow,
                      const QString &sessionPath,
                      const QVariantMap &options);
    QVariantMap Stop(const QString &handle,
                     const QString &appId,
                     const QString &parentWindow,
                     const QString &sessionPath,
                     const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalGlobalShortcutsAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.GlobalShortcuts")

public:
    explicit ImplPortalGlobalShortcutsAdaptor(ImplPortalService *service);

public slots:
    QVariantMap CreateSession(const QString &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QVariantMap &options);
    QVariantMap BindShortcuts(const QString &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QString &sessionPath,
                              const QVariantMap &options);
    QVariantMap ListBindings(const QString &handle,
                             const QString &appId,
                             const QString &parentWindow,
                             const QString &sessionPath,
                             const QVariantMap &options);
    QVariantMap Activate(const QString &handle,
                         const QString &appId,
                         const QString &parentWindow,
                         const QString &sessionPath,
                         const QVariantMap &options);
    QVariantMap Deactivate(const QString &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QString &sessionPath,
                           const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalInputCaptureAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.InputCapture")

public:
    explicit ImplPortalInputCaptureAdaptor(ImplPortalService *service);

public slots:
    QVariantMap CreateSession(const QString &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QVariantMap &options);
    QVariantMap SetPointerBarriers(const QString &handle,
                                   const QString &appId,
                                   const QString &parentWindow,
                                   const QString &sessionPath,
                                   const QVariantMap &options);
    QVariantMap Enable(const QString &handle,
                       const QString &appId,
                       const QString &parentWindow,
                       const QString &sessionPath,
                       const QVariantMap &options);
    QVariantMap Disable(const QString &handle,
                        const QString &appId,
                        const QString &parentWindow,
                        const QString &sessionPath,
                        const QVariantMap &options);
    QVariantMap Release(const QString &handle,
                        const QString &appId,
                        const QString &parentWindow,
                        const QString &sessionPath,
                        const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalSettingsAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")

public:
    explicit ImplPortalSettingsAdaptor(ImplPortalService *service);

public slots:
    QVariantMap ReadAll(const QStringList &namespaces);
    QDBusVariant Read(const QString &settingNamespace, const QString &key);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalNotificationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Notification")

public:
    explicit ImplPortalNotificationAdaptor(ImplPortalService *service);

public slots:
    QVariantMap AddNotification(const QString &appId, const QString &id, const QVariantMap &notification);
    QVariantMap RemoveNotification(const QString &appId, const QString &id);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalInhibitAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Inhibit")

public:
    explicit ImplPortalInhibitAdaptor(ImplPortalService *service);

public slots:
    QVariantMap Inhibit(const QString &handle,
                        const QString &appId,
                        const QString &window,
                        uint flags,
                        const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalOpenWithAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.OpenWith")

public:
    explicit ImplPortalOpenWithAdaptor(ImplPortalService *service);

public slots:
    QVariantMap QueryHandlers(const QString &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QString &path,
                              const QVariantMap &options);
    QVariantMap OpenFileWith(const QString &handle,
                             const QString &appId,
                             const QString &parentWindow,
                             const QString &path,
                             const QString &handlerId,
                             const QVariantMap &options);
    QVariantMap OpenUriWith(const QString &handle,
                            const QString &appId,
                            const QString &parentWindow,
                            const QString &uri,
                            const QString &handlerId,
                            const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalDocumentsAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Documents")

public:
    explicit ImplPortalDocumentsAdaptor(ImplPortalService *service);

public slots:
    QVariantMap Add(const QString &handle,
                    const QString &appId,
                    const QString &parentWindow,
                    const QStringList &uris,
                    const QVariantMap &options);
    QVariantMap Resolve(const QString &handle,
                        const QString &appId,
                        const QString &parentWindow,
                        const QString &token,
                        const QVariantMap &options);
    QVariantMap List(const QString &handle,
                     const QString &appId,
                     const QString &parentWindow,
                     const QVariantMap &options);
    QVariantMap Remove(const QString &handle,
                       const QString &appId,
                       const QString &parentWindow,
                       const QString &token,
                       const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};

class ImplPortalTrashAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Trash")

public:
    explicit ImplPortalTrashAdaptor(ImplPortalService *service);

public slots:
    QVariantMap TrashFile(const QString &handle,
                          const QString &appId,
                          const QString &parentWindow,
                          const QString &path,
                          const QVariantMap &options);
    QVariantMap EmptyTrash(const QString &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QVariantMap &options);

private:
    ImplPortalService *m_service = nullptr;
};
