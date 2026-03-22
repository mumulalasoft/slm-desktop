#include "PolicyDecision.h"

namespace Slm::Permissions {

QString decisionTypeToString(DecisionType type)
{
    switch (type) {
    case DecisionType::Allow:
        return QStringLiteral("allow");
    case DecisionType::Deny:
        return QStringLiteral("deny");
    case DecisionType::AskUser:
        return QStringLiteral("ask_user");
    case DecisionType::AllowOnce:
        return QStringLiteral("allow_once");
    case DecisionType::AllowAlways:
        return QStringLiteral("allow_always");
    case DecisionType::DenyAlways:
    default:
        return QStringLiteral("deny_always");
    }
}

DecisionType decisionTypeFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("allow")) return DecisionType::Allow;
    if (normalized == QStringLiteral("deny")) return DecisionType::Deny;
    if (normalized == QStringLiteral("ask_user")) return DecisionType::AskUser;
    if (normalized == QStringLiteral("allow_once")) return DecisionType::AllowOnce;
    if (normalized == QStringLiteral("allow_always")) return DecisionType::AllowAlways;
    return DecisionType::DenyAlways;
}

QVariantMap policyDecisionToVariantMap(const PolicyDecision &decision)
{
    return {
        {QStringLiteral("decision"), decisionTypeToString(decision.type)},
        {QStringLiteral("capability"), capabilityToString(decision.capability)},
        {QStringLiteral("reason"), decision.reason},
        {QStringLiteral("persistentEligible"), decision.persistentEligible},
        {QStringLiteral("allowed"), decision.isAllowed()},
    };
}

} // namespace Slm::Permissions

