#pragma once

#include "slmactioncachelayer.h"
#include "slmcapabilitybuilders.h"
#include "slmcapabilityresolverlayer.h"
#include "slmdesktopentryparsermodule.h"
#include "slmdesktopentryscanner.h"
#include "slmmetadataparser.h"
#include "slmactioninvoker.h"
#include "slmactionregistryindex.h"

#include <QObject>

namespace Slm::Actions {
class ActionRegistry;
}

namespace Slm::Actions::Framework {

class SlmActionFramework : public QObject
{
    Q_OBJECT

public:
    explicit SlmActionFramework(ActionRegistry *registry, QObject *parent = nullptr);

    void setScanRoots(const QStringList &roots);
    void start();
    void rescanNow();

    QVariantList listActions(const QString &capability, const QVariantMap &context) const;
    QVariantMap invokeAction(const QString &actionId, const QVariantMap &context) const;
    QVariantMap resolveDefaultAction(const QString &capability, const QVariantMap &context) const;
    QVariantList searchActions(const QString &query, const QVariantMap &context) const;

    QVariantList buildContextMenu(const QVariantList &flatActions) const;
    QVariantList buildShareSheet(const QVariantList &flatActions) const;
    QVariantList buildQuickActions(const QVariantList &flatActions) const;
    QVariantList buildSearchResults(const QVariantList &flatActions) const;

signals:
    void scannerRescanned(const QStringList &paths);

private slots:
    void onEntryChanged(const QString &path);
    void onEntryRemoved(const QString &path);
    void onScanCompleted(const QStringList &paths);

private:
    void rebuildIndexes(const QStringList &desktopFiles);

    ActionRegistry *m_registry = nullptr;
    DesktopEntryScannerGio m_scanner;
    DesktopEntryParserModule m_parserModule;
    SlmMetadataParser m_metadataParser;
    ActionRegistryIndex m_registryIndex;
    ActionCacheLayer m_cache;
    CapabilityResolverLayer m_resolver;
    ContextMenuBuilder m_contextMenuBuilder;
    ShareSheetBuilder m_shareSheetBuilder;
    QuickActionBuilder m_quickActionBuilder;
    SearchActionRanker m_searchActionRanker;
    DragDropResolver m_dragDropResolver;
    ActionInvoker m_invoker;
};

} // namespace Slm::Actions::Framework
