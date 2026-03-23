#pragma once

#include <QObject>
#include <QDBusContext>
#include <QVariantMap>

class PortalManager;

class PortalService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Portal")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit PortalService(PortalManager *manager,
                           QObject *backend = nullptr,
                           QObject *parent = nullptr);
    ~PortalService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    QVariantMap Screenshot(const QVariantMap &options);
    QVariantMap ScreenCast(const QVariantMap &options);
    QVariantMap FileChooser(const QVariantMap &options);
    QVariantMap OpenURI(const QString &uri, const QVariantMap &options);
    QVariantMap PickColor(const QVariantMap &options);
    QVariantMap PickFolder(const QVariantMap &options);
    QVariantMap Wallpaper(const QVariantMap &options);

    QVariantMap RequestClipboardPreview(const QVariantMap &options);
    QVariantMap RequestClipboardContent(const QVariantMap &options);
    QVariantMap ResolveClipboardResult(const QString &resultId, const QVariantMap &options);
    QVariantMap QueryClipboardPreview(const QString &query, const QVariantMap &options);
    QVariantMap QueryFiles(const QString &query, const QVariantMap &options);
    QVariantMap QueryContacts(const QString &query, const QVariantMap &options);
    QVariantMap QueryEmailMetadata(const QString &query, const QVariantMap &options);
    QVariantMap QueryMailMetadata(const QString &query, const QVariantMap &options);
    QVariantMap QueryCalendarEvents(const QString &query, const QVariantMap &options);
    QVariantMap ResolveContact(const QString &resultId, const QVariantMap &options);
    QVariantMap ResolveEmailBody(const QString &resultId, const QVariantMap &options);
    QVariantMap ResolveCalendarEvent(const QString &resultId, const QVariantMap &options);
    QVariantMap ReadContact(const QString &contactId, const QVariantMap &options);
    QVariantMap ReadCalendarEvent(const QString &eventId, const QVariantMap &options);
    QVariantMap ReadMailBody(const QString &messageId, const QVariantMap &options);
    QVariantMap PickContact(const QVariantMap &options);
    QVariantMap PickCalendarEvent(const QVariantMap &options);
    QVariantMap QueryActions(const QString &query, const QVariantMap &options);
    QVariantMap QueryContextActions(const QString &target,
                                    const QVariantList &uris,
                                    const QVariantMap &options);
    QVariantMap InvokeAction(const QString &actionId, const QVariantMap &options);
    QVariantMap QueryNotificationHistoryPreview(const QString &query, const QVariantMap &options);
    QVariantMap ReadNotificationHistoryItem(const QString &itemId, const QVariantMap &options);
    QVariantMap RequestShare(const QVariantMap &options);
    QVariantMap ShareItems(const QVariantMap &options);
    QVariantMap ShareText(const QVariantMap &options);
    QVariantMap ShareFiles(const QVariantMap &options);
    QVariantMap ShareUri(const QVariantMap &options);
    QVariantMap CaptureScreen(const QVariantMap &options);
    QVariantMap CaptureWindow(const QVariantMap &options);
    QVariantMap CaptureArea(const QVariantMap &options);
    QVariantMap GlobalShortcutsCreateSession(const QVariantMap &options);
    QVariantMap GlobalShortcutsBindShortcuts(const QString &sessionPath, const QVariantMap &options);
    QVariantMap GlobalShortcutsListBindings(const QString &sessionPath, const QVariantMap &options);
    QVariantMap GlobalShortcutsActivate(const QString &sessionPath, const QVariantMap &options);
    QVariantMap GlobalShortcutsDeactivate(const QString &sessionPath, const QVariantMap &options);
    QVariantMap ScreencastSelectSources(const QString &sessionPath, const QVariantMap &options);
    QVariantMap ScreencastStart(const QString &sessionPath, const QVariantMap &options);
    QVariantMap ScreencastStop(const QString &sessionPath, const QVariantMap &options);
    QVariantMap StartScreencastSession(const QVariantMap &options);
    QVariantMap GetScreencastSessionStreams(const QString &sessionPath);
    QVariantMap CloseSession(const QString &sessionPath);

signals:
    void serviceRegisteredChanged();

private:
    void registerDbusService();
    QVariantMap callBackendRequest(const QString &portalMethod, const QVariantMap &options);
    QVariantMap callBackendDirect(const QString &portalMethod, const QVariantMap &options);
    QVariantMap callBackendSessionRequest(const QString &portalMethod, const QVariantMap &options);
    QVariantMap callBackendSessionOperation(const QString &portalMethod,
                                            const QString &sessionPath,
                                            const QVariantMap &options,
                                            bool requireActive);
    QVariantMap callBackendCloseSession(const QString &sessionPath);

    PortalManager *m_manager = nullptr;
    QObject *m_backend = nullptr;
    bool m_serviceRegistered = false;
};
