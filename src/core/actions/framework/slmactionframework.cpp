#include "slmactionframework.h"

#include "../slmactionregistry.h"

namespace Slm::Actions::Framework {

SlmActionFramework::SlmActionFramework(ActionRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_resolver(registry)
    , m_contextMenuBuilder(registry)
    , m_shareSheetBuilder(registry)
    , m_searchActionRanker(registry)
    , m_dragDropResolver(registry)
    , m_invoker(registry)
{
    connect(&m_scanner, &DesktopEntryScannerGio::entryAdded,
            this, &SlmActionFramework::onEntryChanged);
    connect(&m_scanner, &DesktopEntryScannerGio::entryChanged,
            this, &SlmActionFramework::onEntryChanged);
    connect(&m_scanner, &DesktopEntryScannerGio::entryRemoved,
            this, &SlmActionFramework::onEntryRemoved);
    connect(&m_scanner, &DesktopEntryScannerGio::scanCompleted,
            this, &SlmActionFramework::onScanCompleted);
}

void SlmActionFramework::setScanRoots(const QStringList &roots)
{
    m_scanner.setRoots(roots);
}

void SlmActionFramework::start()
{
    m_scanner.start();
}

void SlmActionFramework::rescanNow()
{
    m_scanner.rescanNow();
}

QVariantList SlmActionFramework::listActions(const QString &capability,
                                             const QVariantMap &context) const
{
    return m_resolver.resolve(capability, context);
}

QVariantMap SlmActionFramework::invokeAction(const QString &actionId,
                                             const QVariantMap &context) const
{
    return m_invoker.invoke(actionId, context);
}

QVariantMap SlmActionFramework::resolveDefaultAction(const QString &capability,
                                                     const QVariantMap &context) const
{
    if (!m_registry) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("registry-unavailable")},
        };
    }
    return m_registry->resolveDefaultAction(capability, context);
}

QVariantList SlmActionFramework::searchActions(const QString &query,
                                               const QVariantMap &context) const
{
    if (!m_registry) {
        return {};
    }
    return m_registry->searchActions(query, context);
}

QVariantList SlmActionFramework::buildContextMenu(const QVariantList &flatActions) const
{
    return m_contextMenuBuilder.build(flatActions);
}

QVariantList SlmActionFramework::buildShareSheet(const QVariantList &flatActions) const
{
    return m_shareSheetBuilder.build(flatActions);
}

QVariantList SlmActionFramework::buildQuickActions(const QVariantList &flatActions) const
{
    return m_quickActionBuilder.build(flatActions);
}

QVariantList SlmActionFramework::buildSearchResults(const QVariantList &flatActions) const
{
    return m_searchActionRanker.rank(flatActions);
}

void SlmActionFramework::onEntryChanged(const QString & /*path*/)
{
    if (m_registry) {
        m_registry->reload();
    }
}

void SlmActionFramework::onEntryRemoved(const QString & /*path*/)
{
    if (m_registry) {
        m_registry->reload();
    }
}

void SlmActionFramework::onScanCompleted(const QStringList &paths)
{
    rebuildIndexes(paths);
    emit scannerRescanned(paths);
}

void SlmActionFramework::rebuildIndexes(const QStringList &desktopFiles)
{
    QVector<ActionDefinition> merged;
    QHash<QString, QSharedPointer<ConditionAstNode>> astByActionId;

    for (const QString &path : desktopFiles) {
        const DesktopParseResult parsed = m_parserModule.parseFile(path);
        if (!parsed.ok) {
            continue;
        }
        const MetadataParseOutput metadata = m_metadataParser.parse(parsed.actions);
        merged += metadata.actions;
        for (auto it = metadata.conditionAstByActionId.cbegin();
             it != metadata.conditionAstByActionId.cend(); ++it) {
            astByActionId.insert(it.key(), it.value());
            m_cache.putConditionAst(it.key(), it.value());
        }
    }

    m_registryIndex.rebuild(merged);
}

} // namespace Slm::Actions::Framework
