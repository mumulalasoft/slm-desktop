#pragma once

#include "../slmactioncondition.h"

#include <QDateTime>
#include <QHash>
#include <QString>

namespace Slm::Actions::Framework {

class ActionCacheLayer
{
public:
    void clear();

    void putParsedMtime(const QString &desktopPath, const QDateTime &mtimeUtc);
    bool isParsedFresh(const QString &desktopPath, const QDateTime &mtimeUtc) const;

    void putConditionAst(const QString &expression, const QSharedPointer<ConditionAstNode> &ast);
    QSharedPointer<ConditionAstNode> conditionAst(const QString &expression) const;

private:
    QHash<QString, QDateTime> m_parsedMtimeUtc;
    QHash<QString, QSharedPointer<ConditionAstNode>> m_conditionAstCache;
};

} // namespace Slm::Actions::Framework

