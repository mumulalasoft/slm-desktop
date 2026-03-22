#pragma once

#include "slmactioncachelayer.h"

#include <QFileInfo>

namespace Slm::Actions::Framework {

class CacheManager
{
public:
    void clear();
    bool shouldReparse(const QString &desktopFilePath) const;
    void markParsed(const QString &desktopFilePath);

    void putConditionAst(const QString &expression, const QSharedPointer<ConditionAstNode> &ast);
    QSharedPointer<ConditionAstNode> conditionAst(const QString &expression) const;

private:
    ActionCacheLayer m_cache;
};

} // namespace Slm::Actions::Framework

