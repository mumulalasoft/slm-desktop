#pragma once

#include "Capability.h"

#include <QString>
#include <QVariantMap>

namespace Slm::Permissions {

enum class DecisionType {
    Allow = 0,
    Deny,
    AskUser,
    AllowOnce,
    AllowAlways,
    DenyAlways
};

struct PolicyDecision {
    DecisionType type = DecisionType::Deny;
    Capability capability = Capability::Unknown;
    QString reason;
    bool persistentEligible = false;

    bool isAllowed() const
    {
        return type == DecisionType::Allow
               || type == DecisionType::AllowOnce
               || type == DecisionType::AllowAlways;
    }
};

QString decisionTypeToString(DecisionType type);
DecisionType decisionTypeFromString(const QString &value);
QVariantMap policyDecisionToVariantMap(const PolicyDecision &decision);

} // namespace Slm::Permissions

