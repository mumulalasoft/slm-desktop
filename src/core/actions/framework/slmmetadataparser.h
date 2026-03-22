#pragma once

#include "../slmactioncondition.h"
#include "../slmactiontypes.h"

#include <QHash>
#include <QVector>

namespace Slm::Actions::Framework {

struct MetadataParseOutput {
    QVector<ActionDefinition> actions;
    QHash<QString, QSharedPointer<ConditionAstNode>> conditionAstByActionId;
    QStringList errors;
};

class SlmMetadataParser
{
public:
    MetadataParseOutput parse(const QVector<ActionDefinition> &actions) const;

private:
    ActionConditionParser m_conditionParser;
};

} // namespace Slm::Actions::Framework

