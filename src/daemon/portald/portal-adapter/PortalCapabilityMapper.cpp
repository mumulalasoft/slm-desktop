#include "PortalCapabilityMapper.h"

namespace Slm::PortalAdapter {

PortalCapabilityMapper::PortalCapabilityMapper(QObject *parent)
    : QObject(parent)
{
}

PortalMethodSpec PortalCapabilityMapper::mapMethod(const QString &portalMethod) const
{
    const QString m = portalMethod.trimmed();
    PortalMethodSpec spec;
    spec.method = m;

    if (m.compare(QStringLiteral("RequestClipboardPreview"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryClipboardPreview;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("RequestClipboardContent"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ClipboardReadHistoryContent;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("ResolveClipboardResult"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchResolveClipboardResult;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryNotificationHistoryPreview"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::NotificationsReadHistory;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("ReadNotificationHistoryItem"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::NotificationsReadHistory;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("RequestShare"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ShareInvoke;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("ShareItems"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("ShareText"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("ShareFiles"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("ShareUri"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ShareInvoke;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("CaptureScreen"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ScreenshotCaptureScreen;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("CaptureWindow"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ScreenshotCaptureWindow;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("CaptureArea"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ScreenshotCaptureArea;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::OneShot;
        return spec;
    }
    if (m.compare(QStringLiteral("GlobalShortcutsCreateSession"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::GlobalShortcutsCreateSession;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("GlobalShortcutsBindShortcuts"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::GlobalShortcutsRegister;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("GlobalShortcutsListBindings"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::GlobalShortcutsList;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Low;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("GlobalShortcutsActivate"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::GlobalShortcutsActivate;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("GlobalShortcutsDeactivate"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::GlobalShortcutsDeactivate;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("InputCaptureCreateSession"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::InputCaptureCreateSession;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.persistenceAllowed = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("InputCaptureSetPointerBarriers"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::InputCaptureSetPointerBarriers;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("InputCaptureEnable"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::InputCaptureEnable;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("InputCaptureDisable"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::InputCaptureDisable;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("InputCaptureRelease"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::InputCaptureRelease;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("StartScreencastSession"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ScreencastCreateSession;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.persistenceAllowed = true;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("ScreencastSelectSources"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ScreencastSelectSources;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("ScreencastStart"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ScreencastStart;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        spec.persistenceAllowed = true;
        return spec;
    }
    if (m.compare(QStringLiteral("ScreencastStop"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::ScreencastStop;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::Session;
        spec.revocableSession = true;
        return spec;
    }
    if (m.compare(QStringLiteral("SearchClipboardPreview"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryClipboardPreview;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryClipboardPreview"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryClipboardPreview;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryFiles"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryFiles;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Low;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryContacts"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryContacts;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryEmailMetadata"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryEmailMetadata;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryMailMetadata"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryEmailMetadata;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryCalendarEvents"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::AccountsReadCalendar;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryActions"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryFiles;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Low;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("QueryContextActions"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::SearchQueryFiles;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.directResponse = true;
        return spec;
    }
    if (m.compare(QStringLiteral("InvokeAction"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::QuickActionInvoke;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Medium;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.persistenceAllowed = true;
        return spec;
    }
    if (m.compare(QStringLiteral("ResolveContact"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("ReadContact"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("PickContact"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::AccountsReadContacts;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.persistenceAllowed = true;
        return spec;
    }
    if (m.compare(QStringLiteral("ResolveCalendarEvent"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("ReadCalendarEvent"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("PickCalendarEvent"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::AccountsReadCalendar;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::High;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.persistenceAllowed = true;
        return spec;
    }
    if (m.compare(QStringLiteral("ResolveEmailBody"), Qt::CaseInsensitive) == 0
        || m.compare(QStringLiteral("ReadMailBody"), Qt::CaseInsensitive) == 0) {
        spec.valid = true;
        spec.capability = Slm::Permissions::Capability::AccountsReadMailBody;
        spec.sensitivity = Slm::Permissions::SensitivityLevel::Critical;
        spec.requiresUserGesture = true;
        spec.requestKind = PortalRequestKind::OneShot;
        spec.persistenceAllowed = true;
        return spec;
    }
    return spec;
}

} // namespace Slm::PortalAdapter
