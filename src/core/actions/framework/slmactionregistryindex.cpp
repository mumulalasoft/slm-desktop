#include "slmactionregistryindex.h"

namespace Slm::Actions::Framework {

void ActionRegistryIndex::clear()
{
    m_byId.clear();
    m_byCapability.clear();
    m_byMime.clear();
    m_byKeyword.clear();
}

void ActionRegistryIndex::rebuild(const QVector<ActionDefinition> &actions)
{
    clear();
    for (const ActionDefinition &action : actions) {
        m_byId.insert(action.id, action);
        for (const Capability capability : action.capabilities) {
            m_byCapability.insert(static_cast<int>(capability), action.id);
        }
        for (const QString &mime : action.mimeTypes) {
            const QString key = mime.trimmed().toLower();
            if (!key.isEmpty()) {
                m_byMime.insert(key, action.id);
            }
        }
        for (const QString &keyword : action.keywords) {
            const QString key = keyword.trimmed().toLower();
            if (!key.isEmpty()) {
                m_byKeyword.insert(key, action.id);
            }
        }
    }
}

bool ActionRegistryIndex::contains(const QString &id) const
{
    return m_byId.contains(id);
}

ActionDefinition ActionRegistryIndex::byId(const QString &id) const
{
    return m_byId.value(id);
}

QVector<ActionDefinition> ActionRegistryIndex::all() const
{
    return m_byId.values().toVector();
}

QVector<ActionDefinition> ActionRegistryIndex::resolveIds(const QList<QString> &ids) const
{
    QVector<ActionDefinition> out;
    QSet<QString> seen;
    out.reserve(ids.size());
    for (const QString &id : ids) {
        if (seen.contains(id) || !m_byId.contains(id)) {
            continue;
        }
        seen.insert(id);
        out.push_back(m_byId.value(id));
    }
    return out;
}

QVector<ActionDefinition> ActionRegistryIndex::byCapability(Capability capability) const
{
    return resolveIds(m_byCapability.values(static_cast<int>(capability)));
}

QVector<ActionDefinition> ActionRegistryIndex::byMime(const QString &mime) const
{
    return resolveIds(m_byMime.values(mime.trimmed().toLower()));
}

QVector<ActionDefinition> ActionRegistryIndex::byKeyword(const QString &keyword) const
{
    return resolveIds(m_byKeyword.values(keyword.trimmed().toLower()));
}

} // namespace Slm::Actions::Framework

