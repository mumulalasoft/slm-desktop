#include "slmcachemanager.h"

namespace Slm::Actions::Framework {

void CacheManager::clear()
{
    m_cache.clear();
}

bool CacheManager::shouldReparse(const QString &desktopFilePath) const
{
    const QFileInfo fi(desktopFilePath);
    if (!fi.exists() || !fi.isFile()) {
        return false;
    }
    return !m_cache.isParsedFresh(fi.absoluteFilePath(), fi.lastModified().toUTC());
}

void CacheManager::markParsed(const QString &desktopFilePath)
{
    const QFileInfo fi(desktopFilePath);
    if (!fi.exists() || !fi.isFile()) {
        return;
    }
    m_cache.putParsedMtime(fi.absoluteFilePath(), fi.lastModified().toUTC());
}

void CacheManager::putConditionAst(const QString &expression,
                                   const QSharedPointer<ConditionAstNode> &ast)
{
    m_cache.putConditionAst(expression, ast);
}

QSharedPointer<ConditionAstNode> CacheManager::conditionAst(const QString &expression) const
{
    return m_cache.conditionAst(expression);
}

} // namespace Slm::Actions::Framework

