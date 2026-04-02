#include "Capability.h"

#include <QHash>

namespace Slm::Permissions {
namespace {

using Entry = QPair<Capability, QString>;

const QList<Entry> &capabilityTable()
{
    static const QList<Entry> table{
        {Capability::ClipboardReadCurrent, QStringLiteral("Clipboard.ReadCurrent")},
        {Capability::ClipboardWriteCurrent, QStringLiteral("Clipboard.WriteCurrent")},
        {Capability::ClipboardReadHistoryPreview, QStringLiteral("Clipboard.ReadHistoryPreview")},
        {Capability::ClipboardReadHistoryContent, QStringLiteral("Clipboard.ReadHistoryContent")},
        {Capability::ClipboardDeleteHistory, QStringLiteral("Clipboard.DeleteHistory")},
        {Capability::ClipboardPinItem, QStringLiteral("Clipboard.PinItem")},
        {Capability::ClipboardClearHistory, QStringLiteral("Clipboard.ClearHistory")},

        {Capability::SearchQueryApps, QStringLiteral("Search.QueryApps")},
        {Capability::SearchQueryFiles, QStringLiteral("Search.QueryFiles")},
        {Capability::SearchQueryClipboardPreview, QStringLiteral("Search.QueryClipboardPreview")},
        {Capability::SearchResolveClipboardResult, QStringLiteral("Search.ResolveClipboardResult")},
        {Capability::SearchQueryContacts, QStringLiteral("Search.QueryContacts")},
        {Capability::SearchQueryEmailMetadata, QStringLiteral("Search.QueryEmailMetadata")},
        {Capability::SearchQueryEmailBody, QStringLiteral("Search.QueryEmailBody")},

        {Capability::ShareInvoke, QStringLiteral("Share.Invoke")},
        {Capability::OpenWithInvoke, QStringLiteral("OpenWith.Invoke")},
        {Capability::QuickActionInvoke, QStringLiteral("QuickAction.Invoke")},
        {Capability::FileActionInvoke, QStringLiteral("FileAction.Invoke")},

        {Capability::AccountsReadContacts, QStringLiteral("Accounts.ReadContacts")},
        {Capability::AccountsReadCalendar, QStringLiteral("Accounts.ReadCalendar")},
        {Capability::AccountsReadMailMetadata, QStringLiteral("Accounts.ReadMailMetadata")},
        {Capability::AccountsReadMailBody, QStringLiteral("Accounts.ReadMailBody")},

        {Capability::ScreenshotCaptureScreen, QStringLiteral("Screenshot.CaptureScreen")},
        {Capability::ScreenshotCaptureWindow, QStringLiteral("Screenshot.CaptureWindow")},
        {Capability::ScreenshotCaptureArea, QStringLiteral("Screenshot.CaptureArea")},
        {Capability::ScreencastStart, QStringLiteral("Screencast.Start")},
        {Capability::ScreencastCreateSession, QStringLiteral("Screencast.CreateSession")},
        {Capability::ScreencastSelectSources, QStringLiteral("Screencast.SelectSources")},
        {Capability::ScreencastStop, QStringLiteral("Screencast.Stop")},

        {Capability::NotificationsSend, QStringLiteral("Notifications.Send")},
        {Capability::NotificationsReadHistory, QStringLiteral("Notifications.ReadHistory")},
        {Capability::GlobalShortcutsCreateSession, QStringLiteral("GlobalShortcuts.CreateSession")},
        {Capability::GlobalShortcutsRegister, QStringLiteral("GlobalShortcuts.Register")},
        {Capability::GlobalShortcutsList, QStringLiteral("GlobalShortcuts.List")},
        {Capability::GlobalShortcutsActivate, QStringLiteral("GlobalShortcuts.Activate")},
        {Capability::GlobalShortcutsDeactivate, QStringLiteral("GlobalShortcuts.Deactivate")},
        {Capability::InputCaptureCreateSession, QStringLiteral("InputCapture.CreateSession")},
        {Capability::InputCaptureSetPointerBarriers, QStringLiteral("InputCapture.SetPointerBarriers")},
        {Capability::InputCaptureEnable, QStringLiteral("InputCapture.Enable")},
        {Capability::InputCaptureDisable, QStringLiteral("InputCapture.Disable")},
        {Capability::InputCaptureRelease, QStringLiteral("InputCapture.Release")},

        {Capability::SessionInhibit, QStringLiteral("Session.Inhibit")},
        {Capability::SessionLogout, QStringLiteral("Session.Logout")},
        {Capability::SessionShutdown, QStringLiteral("Session.Shutdown")},
        {Capability::SessionReboot, QStringLiteral("Session.Reboot")},
        {Capability::SessionLock, QStringLiteral("Session.Lock")},
        {Capability::SessionUnlock, QStringLiteral("Session.Unlock")},

        {Capability::GlobalMenuInvoke, QStringLiteral("GlobalMenu.Invoke")},
        {Capability::GlobalMenuConfigure, QStringLiteral("GlobalMenu.Configure")},

        {Capability::SearchConfigure, QStringLiteral("Search.Configure")},
        {Capability::SearchReadTelemetry, QStringLiteral("Search.ReadTelemetry")},

        {Capability::WorkspaceManage, QStringLiteral("Workspace.Manage")},
        {Capability::DiagnosticsRead, QStringLiteral("Diagnostics.Read")},

        {Capability::DeviceMount, QStringLiteral("Device.Mount")},
        {Capability::DeviceEject, QStringLiteral("Device.Eject")},
        {Capability::DeviceUnlock, QStringLiteral("Device.Unlock")},
        {Capability::DeviceFormat, QStringLiteral("Device.Format")},

        {Capability::PortalFileChooser, QStringLiteral("Portal.FileChooser")},
        {Capability::PortalOpenURI, QStringLiteral("Portal.OpenURI")},
        {Capability::PortalScreenshot, QStringLiteral("Portal.Screenshot")},
        {Capability::PortalScreencastManage, QStringLiteral("Portal.ScreencastManage")},
        {Capability::PortalNotificationSend, QStringLiteral("Portal.NotificationSend")},
        {Capability::PortalInhibit, QStringLiteral("Portal.Inhibit")},
        {Capability::PortalOpenWith, QStringLiteral("Portal.OpenWith")},
        {Capability::SecretStore, QStringLiteral("Secret.Store")},
        {Capability::SecretRead, QStringLiteral("Secret.Read")},
        {Capability::SecretDelete, QStringLiteral("Secret.Delete")},
    };
    return table;
}

} // namespace

QString capabilityToString(Capability capability)
{
    for (const Entry &entry : capabilityTable()) {
        if (entry.first == capability) {
            return entry.second;
        }
    }
    return QStringLiteral("Unknown");
}

Capability capabilityFromString(const QString &value)
{
    const QString normalized = value.trimmed();
    for (const Entry &entry : capabilityTable()) {
        if (entry.second.compare(normalized, Qt::CaseInsensitive) == 0) {
            return entry.first;
        }
    }
    return Capability::Unknown;
}

QList<Capability> allCapabilities()
{
    QList<Capability> out;
    out.reserve(capabilityTable().size());
    for (const Entry &entry : capabilityTable()) {
        out.push_back(entry.first);
    }
    return out;
}

} // namespace Slm::Permissions
