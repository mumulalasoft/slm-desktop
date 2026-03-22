#include "slmactioncachelayer.h"

namespace Slm::Actions::Framework {

void ActionCacheLayer::clear()
{
    m_parsedMtimeUtc.clear();
    m_conditionAstCache.clear();
}

void ActionCacheLayer::putParsedMtime(const QString &desktopPath, const QDateTime &mtimeUtc)
{
    m_parsedMtimeUtc.insert(desktopPath, mtimeUtc);
}

bool ActionCacheLayer::isParsedFresh(const QString &desktopPath, const QDateTime &mtimeUtc) const
{
    return m_parsedMtimeUtc.value(desktopPath).toMSecsSinceEpoch() == mtimeUtc.toMSecsSinceEpoch();
}

void ActionCacheLayer::putConditionAst(const QString &expression,
                                       const QSharedPointer<ConditionAstNode> &ast)
{
    const QString key = expression.trimmed();
    if (key.isEmpty() || ast.isNull()) {
        return;
    }
    m_conditionAstCache.insert(key, ast);
}

QSharedPointer<ConditionAstNode> ActionCacheLayer::conditionAst(const QString &expression) const
{
    return m_conditionAstCache.value(expression.trimmed());
}

} // namespace Slm::Actions::Framework

