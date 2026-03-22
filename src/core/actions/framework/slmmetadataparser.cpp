#include "slmmetadataparser.h"

namespace Slm::Actions::Framework {

MetadataParseOutput SlmMetadataParser::parse(const QVector<ActionDefinition> &actions) const
{
    MetadataParseOutput out;
    out.actions = actions;
    out.conditionAstByActionId.reserve(actions.size());

    for (const ActionDefinition &action : actions) {
        const QString expression = action.conditions.trimmed();
        if (expression.isEmpty()) {
            continue;
        }
        const ConditionParseResult parsed = m_conditionParser.parse(expression);
        if (!parsed.ok) {
            out.errors.push_back(QStringLiteral("%1: %2").arg(action.id, parsed.error));
            continue;
        }
        out.conditionAstByActionId.insert(action.id, parsed.root);
    }
    return out;
}

} // namespace Slm::Actions::Framework

