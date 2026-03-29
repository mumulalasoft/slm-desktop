#pragma once

#include <QList>
#include <QString>

namespace Slm::Permissions {

enum class Capability {
    Unknown = 0,

    ClipboardReadCurrent,
    ClipboardWriteCurrent,
    ClipboardReadHistoryPreview,
    ClipboardReadHistoryContent,
    ClipboardDeleteHistory,
    ClipboardPinItem,
    ClipboardClearHistory,

    SearchQueryApps,
    SearchQueryFiles,
    SearchQueryClipboardPreview,
    SearchResolveClipboardResult,
    SearchQueryContacts,
    SearchQueryEmailMetadata,
    SearchQueryEmailBody,

    ShareInvoke,
    OpenWithInvoke,
    QuickActionInvoke,
    FileActionInvoke,

    AccountsReadContacts,
    AccountsReadCalendar,
    AccountsReadMailMetadata,
    AccountsReadMailBody,

    ScreenshotCaptureScreen,
    ScreenshotCaptureWindow,
    ScreenshotCaptureArea,
    ScreencastStart,
    ScreencastCreateSession,
    ScreencastSelectSources,
    ScreencastStop,

    NotificationsSend,
    NotificationsReadHistory,

    GlobalShortcutsCreateSession,
    GlobalShortcutsRegister,
    GlobalShortcutsList,
    GlobalShortcutsActivate,
    GlobalShortcutsDeactivate,
    InputCaptureCreateSession,
    InputCaptureSetPointerBarriers,
    InputCaptureEnable,
    InputCaptureDisable,
    InputCaptureRelease,

    SessionInhibit,
    SessionLogout,
    SessionShutdown,
    SessionReboot,
    SessionLock,
    SessionUnlock,

    GlobalMenuInvoke,
    GlobalMenuConfigure,

    SearchConfigure,
    SearchReadTelemetry,

    WorkspaceManage,
    DiagnosticsRead,

    DeviceMount,
    DeviceEject,
    DeviceUnlock,
    DeviceFormat,

    PortalFileChooser,
    PortalOpenURI,
    PortalScreenshot,
    PortalScreencastManage,
    PortalNotificationSend,
    PortalInhibit,
    PortalOpenWith,
    SecretStore,
    SecretRead,
    SecretDelete,
};

QString capabilityToString(Capability capability);
Capability capabilityFromString(const QString &value);
QList<Capability> allCapabilities();

} // namespace Slm::Permissions
