#include "PolicyEngine.h"

#include <QSet>

namespace Slm::Permissions {
namespace {

const QSet<Capability> &thirdPartyAlwaysAllow()
{
    static const QSet<Capability> caps{
        Capability::ClipboardWriteCurrent,
        Capability::NotificationsSend,
        Capability::SearchQueryApps,
        Capability::SearchQueryFiles,
        Capability::ScreencastStop,
        Capability::GlobalShortcutsDeactivate,
    };
    return caps;
}

const QSet<Capability> &thirdPartyAskUser()
{
    static const QSet<Capability> caps{
        Capability::ShareInvoke,
        Capability::OpenWithInvoke,
        Capability::QuickActionInvoke,
        Capability::FileActionInvoke,
        Capability::ScreenshotCaptureScreen,
        Capability::ScreenshotCaptureWindow,
        Capability::ScreenshotCaptureArea,
        Capability::ScreencastCreateSession,
        Capability::ScreencastSelectSources,
        Capability::ScreencastStart,
        Capability::GlobalShortcutsCreateSession,
        Capability::GlobalShortcutsRegister,
        Capability::GlobalShortcutsList,
        Capability::GlobalShortcutsActivate,
        Capability::InputCaptureCreateSession,
        Capability::InputCaptureSetPointerBarriers,
        Capability::InputCaptureEnable,
        Capability::InputCaptureDisable,
        Capability::AccountsReadContacts,
        Capability::AccountsReadCalendar,
        Capability::AccountsReadMailMetadata,
        Capability::SecretStore,
        Capability::SecretRead,
        Capability::SecretDelete,
    };
    return caps;
}

} // namespace

PolicyEngine::PolicyEngine()
{
    // Sensitivity defaults.
    for (Capability c : allCapabilities()) {
        m_sensitivity.insert(c, SensitivityLevel::Low);
        m_requiresGesture.insert(c, false);
    }
    m_sensitivity[Capability::SearchQueryClipboardPreview] = SensitivityLevel::Medium;
    m_sensitivity[Capability::SearchQueryContacts] = SensitivityLevel::Medium;
    m_sensitivity[Capability::SearchQueryEmailMetadata] = SensitivityLevel::Medium;
    m_sensitivity[Capability::SearchResolveClipboardResult] = SensitivityLevel::High;
    m_sensitivity[Capability::ClipboardReadHistoryPreview] = SensitivityLevel::Medium;
    m_sensitivity[Capability::AccountsReadContacts] = SensitivityLevel::Medium;
    m_sensitivity[Capability::AccountsReadCalendar] = SensitivityLevel::High;
    m_sensitivity[Capability::AccountsReadMailMetadata] = SensitivityLevel::High;
    m_sensitivity[Capability::SearchQueryEmailBody] = SensitivityLevel::High;
    m_sensitivity[Capability::ClipboardReadHistoryContent] = SensitivityLevel::High;
    m_sensitivity[Capability::AccountsReadMailBody] = SensitivityLevel::High;
    m_sensitivity[Capability::ScreenshotCaptureScreen] = SensitivityLevel::High;
    m_sensitivity[Capability::ScreenshotCaptureWindow] = SensitivityLevel::High;
    m_sensitivity[Capability::ScreenshotCaptureArea] = SensitivityLevel::High;
    m_sensitivity[Capability::ScreencastCreateSession] = SensitivityLevel::High;
    m_sensitivity[Capability::ScreencastSelectSources] = SensitivityLevel::High;
    m_sensitivity[Capability::ScreencastStart] = SensitivityLevel::High;
    m_sensitivity[Capability::GlobalShortcutsCreateSession] = SensitivityLevel::Medium;
    m_sensitivity[Capability::GlobalShortcutsRegister] = SensitivityLevel::Medium;
    m_sensitivity[Capability::GlobalShortcutsActivate] = SensitivityLevel::Medium;
    m_sensitivity[Capability::InputCaptureCreateSession] = SensitivityLevel::High;
    m_sensitivity[Capability::InputCaptureSetPointerBarriers] = SensitivityLevel::High;
    m_sensitivity[Capability::InputCaptureEnable] = SensitivityLevel::High;
    m_sensitivity[Capability::InputCaptureDisable] = SensitivityLevel::Medium;
    m_sensitivity[Capability::InputCaptureRelease] = SensitivityLevel::Medium;
    m_sensitivity[Capability::SecretStore] = SensitivityLevel::High;
    m_sensitivity[Capability::SecretRead] = SensitivityLevel::High;
    m_sensitivity[Capability::SecretDelete] = SensitivityLevel::Medium;

    // Gesture-gated defaults.
    m_requiresGesture[Capability::ClipboardReadHistoryContent] = true;
    m_requiresGesture[Capability::SearchResolveClipboardResult] = true;
    m_requiresGesture[Capability::ShareInvoke] = true;
    m_requiresGesture[Capability::OpenWithInvoke] = true;
    m_requiresGesture[Capability::QuickActionInvoke] = true;
    m_requiresGesture[Capability::FileActionInvoke] = true;
    m_requiresGesture[Capability::ScreenshotCaptureScreen] = true;
    m_requiresGesture[Capability::ScreenshotCaptureWindow] = true;
    m_requiresGesture[Capability::ScreenshotCaptureArea] = true;
    m_requiresGesture[Capability::ScreencastCreateSession] = true;
    m_requiresGesture[Capability::ScreencastSelectSources] = true;
    m_requiresGesture[Capability::ScreencastStart] = true;
    m_requiresGesture[Capability::GlobalShortcutsCreateSession] = true;
    m_requiresGesture[Capability::GlobalShortcutsRegister] = true;
    m_requiresGesture[Capability::GlobalShortcutsActivate] = true;
    m_requiresGesture[Capability::InputCaptureCreateSession] = true;
    m_requiresGesture[Capability::InputCaptureSetPointerBarriers] = true;
    m_requiresGesture[Capability::InputCaptureEnable] = true;
}

void PolicyEngine::setStore(PermissionStore *store)
{
    m_store = store;
}

void PolicyEngine::setSensitiveContentPolicy(const SensitiveContentPolicy &policy)
{
    m_sensitivePolicy = policy;
}

SensitivityLevel PolicyEngine::sensitivityForCapability(Capability capability) const
{
    return m_sensitivity.value(capability, SensitivityLevel::Low);
}

bool PolicyEngine::requiresUserGesture(Capability capability) const
{
    return m_requiresGesture.value(capability, false);
}

DecisionType PolicyEngine::mapStoredDecision(DecisionType stored) const
{
    switch (stored) {
    case DecisionType::AllowAlways:
    case DecisionType::AllowOnce:
        return DecisionType::Allow;
    case DecisionType::DenyAlways:
        return DecisionType::Deny;
    default:
        return stored;
    }
}

PolicyDecision PolicyEngine::evaluate(const CallerIdentity &caller,
                                      const AccessContext &context) const
{
    PolicyDecision out;
    out.capability = context.capability;

    if (context.capability == Capability::Unknown) {
        out.type = DecisionType::Deny;
        out.reason = QStringLiteral("unknown-capability");
        return out;
    }

    if (m_sensitivePolicy.shouldBlock(caller, context)) {
        out.type = DecisionType::Deny;
        out.reason = QStringLiteral("critical-content-blocked");
        return out;
    }

    if (m_store && !caller.appId.trimmed().isEmpty()) {
        const StoredPermission stored = m_store->findPermission(caller.appId,
                                                                context.capability,
                                                                context.resourceType,
                                                                context.resourceId);
        if (stored.valid) {
            out.type = mapStoredDecision(stored.decision);
            out.reason = QStringLiteral("stored-policy");
            return out;
        }
    }

    switch (caller.trustLevel) {
    case TrustLevel::CoreDesktopComponent:
        return evaluateCoreComponent(caller, context);
    case TrustLevel::PrivilegedDesktopService:
        return evaluatePrivilegedService(caller, context);
    case TrustLevel::ThirdPartyApplication:
    default:
        return evaluateThirdParty(caller, context);
    }
}

PolicyDecision PolicyEngine::evaluateCoreComponent(const CallerIdentity &caller,
                                                   const AccessContext &context) const
{
    Q_UNUSED(caller)
    PolicyDecision out;
    out.capability = context.capability;
    if (requiresUserGesture(context.capability)
        && !context.initiatedByUserGesture
        && !context.initiatedFromOfficialUI) {
        out.type = DecisionType::AskUser;
        out.reason = QStringLiteral("gesture-required");
        out.persistentEligible = true;
        return out;
    }
    out.type = DecisionType::Allow;
    out.reason = QStringLiteral("core-component");
    out.persistentEligible = true;
    return out;
}

PolicyDecision PolicyEngine::evaluatePrivilegedService(const CallerIdentity &caller,
                                                       const AccessContext &context) const
{
    PolicyDecision out;
    out.capability = context.capability;
    // Privileged services are allowed, but still constrained by explicit capability.
    if (context.capability == Capability::AccountsReadMailBody
        && !caller.isOfficialComponent) {
        out.type = DecisionType::AskUser;
        out.reason = QStringLiteral("privileged-mail-body-requires-mediation");
        out.persistentEligible = true;
        return out;
    }
    out.type = DecisionType::Allow;
    out.reason = QStringLiteral("privileged-service");
    out.persistentEligible = true;
    return out;
}

PolicyDecision PolicyEngine::evaluateThirdParty(const CallerIdentity &caller,
                                                const AccessContext &context) const
{
    Q_UNUSED(caller)
    PolicyDecision out;
    out.capability = context.capability;

    if (requiresUserGesture(context.capability) && !context.initiatedByUserGesture) {
        out.type = DecisionType::Deny;
        out.reason = QStringLiteral("user-gesture-required");
        return out;
    }

    if (thirdPartyAlwaysAllow().contains(context.capability)) {
        out.type = DecisionType::Allow;
        out.reason = QStringLiteral("third-party-low-risk");
        out.persistentEligible = false;
        return out;
    }
    if (thirdPartyAskUser().contains(context.capability)) {
        out.type = DecisionType::AskUser;
        out.reason = QStringLiteral("third-party-mediation-required");
        out.persistentEligible = true;
        return out;
    }
    out.type = DecisionType::Deny;
    out.reason = QStringLiteral("default-deny");
    out.persistentEligible = false;
    return out;
}

} // namespace Slm::Permissions
