#include "slmcapabilitybuilders.h"

namespace Slm::Actions::Framework {

ContextMenuBuilder::ContextMenuBuilder(ActionRegistry *registry)
    : m_registry(registry)
{
}

QVariantList ContextMenuBuilder::build(const QVariantList &flatActions) const
{
    if (!m_registry) {
        return flatActions;
    }
    return m_registry->buildMenuTree(flatActions);
}

ShareSheetBuilder::ShareSheetBuilder(ActionRegistry *registry)
    : m_registry(registry)
{
}

QVariantList ShareSheetBuilder::build(const QVariantList &flatActions) const
{
    if (!m_registry) {
        return flatActions;
    }
    return m_registry->buildShareSheet(flatActions);
}

QVariantList QuickActionBuilder::build(const QVariantList &flatActions) const
{
    return flatActions;
}

SearchActionRanker::SearchActionRanker(ActionRegistry *registry)
    : m_registry(registry)
{
}

QVariantList SearchActionRanker::rank(const QVariantList &flatActions) const
{
    if (!m_registry) {
        return flatActions;
    }
    return m_registry->buildSearchResults(flatActions);
}

DragDropResolver::DragDropResolver(ActionRegistry *registry)
    : m_registry(registry)
{
}

QVariantMap DragDropResolver::resolveDefault(const QVariantMap &context) const
{
    if (!m_registry) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("registry-unavailable")},
        };
    }
    return m_registry->resolveDefaultAction(QStringLiteral("DragDrop"), context);
}

} // namespace Slm::Actions::Framework
