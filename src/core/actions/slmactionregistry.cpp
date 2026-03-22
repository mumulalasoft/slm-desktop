#include "slmactionregistry.h"
#include "../execution/appexecutiongate.h"

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>
#include <functional>

namespace Slm::Actions {
namespace {

QString normalizeUriPath(const QString &uri)
{
    const QUrl u(uri);
    if (u.isLocalFile()) {
        return QDir::cleanPath(u.toLocalFile());
    }
    if (QFileInfo(uri).exists()) {
        return QDir::cleanPath(uri);
    }
    return QString();
}

QVariantMap makeGroupNode(const QString &path, const QString &label)
{
    QVariantMap node;
    node.insert(QStringLiteral("type"), QStringLiteral("group"));
    node.insert(QStringLiteral("path"), path);
    node.insert(QStringLiteral("label"), label);
    node.insert(QStringLiteral("children"), QVariantList{});
    return node;
}

QVariantMap makeActionNode(const QVariantMap &action)
{
    QVariantMap node;
    node.insert(QStringLiteral("type"), QStringLiteral("action"));
    node.insert(QStringLiteral("action"), action);
    return node;
}

} // namespace

ActionRegistry::ActionRegistry(QObject *parent)
    : QObject(parent)
{
    connect(&m_scanner, &ActionDiscoveryScanner::scanStarted, this, &ActionRegistry::indexingStarted);
    connect(&m_scanner, &ActionDiscoveryScanner::scanFinished, this, &ActionRegistry::indexingFinished);
    connect(&m_scanner, &ActionDiscoveryScanner::cacheUpdated, this, &ActionRegistry::registryUpdated);
    emit providerRegistered(QStringLiteral("desktop-entry"));
    emit providerRegistered(QStringLiteral("slm-context-actions"));
}

void ActionRegistry::setExecutionGate(AppExecutionGate *gate)
{
    m_executionGate = gate;
}

void ActionRegistry::setScanDirectories(const QStringList &directories)
{
    m_scanner.setScanDirectories(directories);
}

QStringList ActionRegistry::scanDirectories() const
{
    return m_scanner.scanDirectories();
}

void ActionRegistry::reload()
{
    m_scanner.scanNow();
}

ActionEvalContext ActionRegistry::buildContext(const QStringList &uris, const QVariantMap &options)
{
    return toEvalContext(parseActionContext(buildContextMap(uris, options)));
}

QVariantMap ActionRegistry::buildContextMap(const QStringList &uris, const QVariantMap &options)
{
    QVariantMap context = options;
    context.insert(QStringLiteral("uris"), uris);
    if (!context.contains(QStringLiteral("selection_count"))) {
        context.insert(QStringLiteral("selection_count"), uris.size());
    }
    return context;
}

ActionContext ActionRegistry::parseActionContext(const QVariantMap &context)
{
    ActionContext out;
    out.text = context.value(QStringLiteral("text")).toString();
    out.scope = context.value(QStringLiteral("scope")).toString().trimmed().toLower();
    out.selectionCount = context.value(QStringLiteral("selection_count")).toInt();
    out.sourceApp = context.value(QStringLiteral("source_app")).toString().trimmed();
    out.extras = context;

    const QVariant urisValue = context.value(QStringLiteral("uris"));
    if (urisValue.canConvert<QStringList>()) {
        out.uris = urisValue.toStringList();
    } else if (urisValue.typeId() == QMetaType::QVariantList) {
        const QVariantList list = urisValue.toList();
        out.uris.reserve(list.size());
        for (const QVariant &entry : list) {
            const QString uri = entry.toString().trimmed();
            if (!uri.isEmpty()) {
                out.uris.push_back(uri);
            }
        }
    }

    const QVariant mimeValue = context.value(QStringLiteral("mime_types"));
    if (mimeValue.canConvert<QStringList>()) {
        out.mimeTypes = mimeValue.toStringList();
    } else if (mimeValue.typeId() == QMetaType::QVariantList) {
        const QVariantList list = mimeValue.toList();
        out.mimeTypes.reserve(list.size());
        for (const QVariant &entry : list) {
            const QString mime = entry.toString().trimmed();
            if (!mime.isEmpty()) {
                out.mimeTypes.push_back(mime);
            }
        }
    }
    if (out.selectionCount <= 0) {
        out.selectionCount = out.uris.size();
    }
    return out;
}

ActionEvalContext ActionRegistry::toEvalContext(const ActionContext &context)
{
    ActionEvalContext ctx;
    ctx.count = context.selectionCount;
    ctx.target = context.extras.value(QStringLiteral("target")).toString().trimmed().toLower();
    QMimeDatabase mimeDb;

    for (const QString &uri : context.uris) {
        ActionItemMeta meta;
        meta.uri = uri;
        const QUrl u(uri);
        meta.scheme = u.scheme();
        meta.path = normalizeUriPath(uri);

        QFileInfo fi(meta.path);
        if (fi.exists()) {
            meta.size = fi.size();
            meta.writable = fi.isWritable();
            meta.executable = fi.isExecutable();
            meta.ext = fi.suffix().toLower();
            if (fi.isSymLink()) {
                meta.target = QStringLiteral("link");
            } else if (fi.isDir()) {
                meta.target = QStringLiteral("directory");
            } else {
                meta.target = QStringLiteral("file");
            }
            meta.mime = mimeDb.mimeTypeForFile(fi, QMimeDatabase::MatchContent).name();
        } else {
            meta.ext = QFileInfo(uri).suffix().toLower();
        }
        ctx.items.push_back(meta);
    }

    if (ctx.target.isEmpty()) {
        if (ctx.count == 0) {
            if (!context.text.trimmed().isEmpty()) {
                ctx.target = QStringLiteral("text");
            } else {
                ctx.target = QStringLiteral("background");
            }
        } else if (ctx.count > 1) {
            ctx.target = QStringLiteral("selection");
        } else {
            ctx.target = ctx.items.first().target;
            if (ctx.target.isEmpty()) {
                ctx.target = QStringLiteral("file");
            }
        }
    }
    if (ctx.target == QStringLiteral("file")
        && context.extras.value(QStringLiteral("uri_list")).toBool()) {
        ctx.target = QStringLiteral("uri-list");
    }
    return ctx;
}

bool ActionRegistry::targetAllowed(const QSet<Target> &targets, const ActionEvalContext &ctx)
{
    if (targets.isEmpty()) {
        return true;
    }
    const Target t = targetFromString(ctx.target);
    if (t == Target::Unknown) {
        return false;
    }
    if (t == Target::Selection && !targets.contains(Target::Selection)) {
        if (ctx.items.isEmpty()) {
            return false;
        }
        // Graceful multi-selection matching:
        // if each selected item matches one of declared targets, accept.
        for (const ActionItemMeta &item : ctx.items) {
            Target itemTarget = targetFromString(item.target);
            if (itemTarget == Target::Unknown) {
                return false;
            }
            // Symlink commonly behaves as file in action handlers.
            if (itemTarget == Target::Link && targets.contains(Target::File)) {
                continue;
            }
            if (!targets.contains(itemTarget)) {
                return false;
            }
        }
        return true;
    }
    return targets.contains(t);
}

bool ActionRegistry::targetAllowed(const FileAction &action, const ActionEvalContext &ctx)
{
    return targetAllowed(action.targets, ctx);
}

bool ActionRegistry::capabilityContextAllowed(const FileAction &action,
                                              Capability requested,
                                              const ActionContext &context,
                                              const ActionEvalContext &evalCtx)
{
    switch (requested) {
    case Capability::ContextMenu:
        return action.contextMenu;
    case Capability::Share:
        if (!action.share.enabled) {
            return false;
        }
        if (!targetAllowed(action.share.targets, evalCtx)) {
            return false;
        }
        return true;
    case Capability::QuickAction: {
        if (!action.quickAction.enabled) {
            return false;
        }
        if (!action.quickAction.scopes.isEmpty()) {
            const QuickActionScope scope = quickActionScopeFromString(context.scope);
            if (scope == QuickActionScope::Unknown
                || !action.quickAction.scopes.contains(scope)) {
                return false;
            }
        }
        if (action.quickAction.visibility == QuickActionVisibility::Contextual
            && context.selectionCount <= 0
            && context.text.trimmed().isEmpty()) {
            return false;
        }
        return true;
    }
    case Capability::DragDrop:
        if (!action.dragDrop.enabled) {
            return false;
        }
        if (!targetAllowed(action.dragDrop.accepts, evalCtx)) {
            return false;
        }
        if (!action.dragDrop.destinations.isEmpty()) {
            const QString dstString = context.extras.value(QStringLiteral("drag_destination"))
                                          .toString().trimmed().toLower();
            DragDropDestination destination = DragDropDestination::Unknown;
            if (dstString == QStringLiteral("app")) {
                destination = DragDropDestination::App;
            } else if (dstString == QStringLiteral("widget")) {
                destination = DragDropDestination::Widget;
            } else if (dstString == QStringLiteral("window")) {
                destination = DragDropDestination::Window;
            } else if (dstString == QStringLiteral("desktop")) {
                destination = DragDropDestination::Desktop;
            }
            if (destination != DragDropDestination::Unknown
                && !action.dragDrop.destinations.contains(destination)) {
                return false;
            }
        }
        return true;
    case Capability::SearchAction:
        if (!action.searchAction.enabled) {
            return false;
        }
        if (!action.searchAction.scopes.isEmpty()) {
            const SearchActionScope scope = searchActionScopeFromString(context.scope);
            if (scope == SearchActionScope::Unknown
                || !action.searchAction.scopes.contains(scope)) {
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}

int ActionRegistry::scoreSearchCandidate(const FileAction &action,
                                         const ActionContext &context,
                                         const QString &query)
{
    const QString q = query.trimmed().toLower();
    const QString title = action.name.toLower();
    const int normalizedPriority = qBound(-200, action.priority, 1000);
    int score = 1400 - (normalizedPriority * 2);
    if (!q.isEmpty()) {
        if (title == q) {
            score += 1000;
        } else if (title.startsWith(q)) {
            score += 500;
        } else if (title.contains(q)) {
            score += 250;
        }
        for (const QString &keyword : action.keywords) {
            const QString k = keyword.toLower();
            if (k == q) {
                score += 300;
            } else if (k.startsWith(q)) {
                score += 180;
            } else if (k.contains(q)) {
                score += 90;
            }
        }
        if (!action.searchAction.intent.trimmed().isEmpty()
            && action.searchAction.intent.trimmed().toLower() == q) {
            score += 400;
        } else if (!action.searchAction.intent.trimmed().isEmpty()
                   && action.searchAction.intent.trimmed().toLower().startsWith(q)) {
            score += 220;
        }
    }
    if (!context.scope.isEmpty()
        && action.searchAction.scopes.contains(searchActionScopeFromString(context.scope))) {
        score += 50;
    }
    return score;
}

QString ActionRegistry::fillExec(const QString &execTemplate, const QStringList &uris)
{
    QString out = execTemplate;
    const QString joinedLocal = [&uris]() {
        QStringList local;
        local.reserve(uris.size());
        for (const QString &uri : uris) {
            const QUrl u(uri);
            if (u.isLocalFile()) {
                local.push_back(u.toLocalFile());
            } else {
                local.push_back(uri);
            }
        }
        return local.join(QLatin1Char(' '));
    }();
    const QString joinedUri = uris.join(QLatin1Char(' '));
    out.replace(QStringLiteral("%F"), joinedLocal);
    out.replace(QStringLiteral("%f"), uris.isEmpty() ? QString() : normalizeUriPath(uris.first()));
    out.replace(QStringLiteral("%U"), joinedUri);
    out.replace(QStringLiteral("%u"), uris.isEmpty() ? QString() : uris.first());
    out.replace(QStringLiteral("%%"), QStringLiteral("%"));
    return out.trimmed();
}

QVariantList ActionRegistry::listActions(const QString &capability,
                                         const QStringList &uris,
                                         const QVariantMap &options)
{
    return listActionsWithContext(capability, buildContextMap(uris, options));
}

QVariantList ActionRegistry::listActionsWithContext(const QString &capability,
                                                    const QVariantMap &context)
{
    const Capability requested = capabilityFromString(capability);
    if (requested == Capability::Unknown) {
        return {};
    }

    if (m_scanner.cachedActions().isEmpty()) {
        m_scanner.scanNow();
    }
    const QVector<FileAction> all = m_scanner.cachedActions();
    const ActionContext parsedContext = parseActionContext(context);
    const ActionEvalContext evalCtx = toEvalContext(parsedContext);

    struct Candidate {
        FileAction action;
    };
    QVector<Candidate> candidates;
    candidates.reserve(all.size());

    for (const FileAction &action : all) {
        if (!action.capabilities.contains(requested)) {
            continue;
        }
        if (!capabilityContextAllowed(action, requested, parsedContext, evalCtx)) {
            continue;
        }
        if (!targetAllowed(action, evalCtx)) {
            continue;
        }
        if (!action.conditions.trimmed().isEmpty()) {
            const ConditionParseResult parsed = m_conditionParser.parse(action.conditions);
            if (!parsed.ok) {
                continue;
            }
            if (!m_conditionEvaluator.evaluate(parsed.root, evalCtx)) {
                continue;
            }
        }
        candidates.push_back({action});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.action.priority != b.action.priority) {
            return a.action.priority < b.action.priority;
        }
        const int byName = QString::localeAwareCompare(a.action.name, b.action.name);
        if (byName != 0) {
            return byName < 0;
        }
        return a.action.id < b.action.id;
    });

    QVariantList out;
    out.reserve(candidates.size());
    for (const Candidate &candidate : candidates) {
        out.push_back(toVariantMap(candidate.action));
    }
    return out;
}

QVariantMap ActionRegistry::invokeAction(const QString &actionId,
                                         const QStringList &uris,
                                         const QVariantMap &options) const
{
    return invokeActionWithContext(actionId, buildContextMap(uris, options));
}

QVariantMap ActionRegistry::invokeActionWithContext(const QString &actionId,
                                                    const QVariantMap &context) const
{
    const QVector<FileAction> all = m_scanner.cachedActions();
    auto it = std::find_if(all.begin(), all.end(), [&actionId](const FileAction &action) {
        return action.id == actionId;
    });
    if (it == all.end()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("action-not-found")},
        };
    }
    const QStringList uris = context.value(QStringLiteral("uris")).toStringList();
    const QString command = fillExec(it->exec, uris);
    if (command.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-exec")},
        };
    }
    if (!m_executionGate) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("execution-gate-unavailable")},
        };
    }
    const QString cwd = context.value(QStringLiteral("cwd")).toString().trimmed();
    const bool started = m_executionGate->launchCommand(command, cwd, QStringLiteral("slm-capability-action"));
    return {
        {QStringLiteral("ok"), started},
        {QStringLiteral("action_id"), actionId},
        {QStringLiteral("command"), command},
    };
}

QVariantMap ActionRegistry::resolveDefaultAction(const QString &capability,
                                                 const QVariantMap &context)
{
    const Capability requested = capabilityFromString(capability);
    if (requested == Capability::Unknown
        && capability.trimmed().compare(QStringLiteral("OpenWith"), Qt::CaseInsensitive) == 0) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("unsupported-capability")}};
    }

    const QVariantList actions = listActionsWithContext(capability, context);
    if (actions.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("no-action")}};
    }
    QVariantMap selected;
    if (requested == Capability::DragDrop) {
        for (const QVariant &row : actions) {
            const QVariantMap action = row.toMap();
            const QString op = action.value(QStringLiteral("dragDrop")).toMap()
                                   .value(QStringLiteral("operation")).toString();
            if (op != QStringLiteral("ask")) {
                selected = action;
                break;
            }
        }
    }
    if (selected.isEmpty()) {
        selected = actions.first().toMap();
    }
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("action"), selected}};
}

QVariantList ActionRegistry::searchActions(const QString &query,
                                           const QVariantMap &context)
{
    QVariantList actions;
    QSet<QString> seenIds;
    const auto appendRows = [&](const QVariantList &rows) {
        for (const QVariant &rowVar : rows) {
            const QVariantMap row = rowVar.toMap();
            const QString id = row.value(QStringLiteral("id")).toString().trimmed();
            if (id.isEmpty() || seenIds.contains(id)) {
                continue;
            }
            seenIds.insert(id);
            actions.push_back(row);
        }
    };
    const auto appendCapability = [&](const QString &capabilityName, bool relaxedForTextOnly) {
        appendRows(listActionsWithContext(capabilityName, context));
        if (!relaxedForTextOnly) {
            return;
        }
        const QStringList uris = context.value(QStringLiteral("uris")).toStringList();
        const int selectionCount = context.value(QStringLiteral("selection_count")).toInt();
        if (!uris.isEmpty() || selectionCount > 0) {
            return;
        }

        QVariantMap relaxedContext = context;
        relaxedContext.insert(QStringLiteral("selection_count"), 1);
        // Text-only search fallback:
        // expose file-oriented actions (e.g. resize/convert) as command candidates.
        const QStringList syntheticTargets{
            QStringLiteral("file"),
            QStringLiteral("selection")
        };
        for (const QString &target : syntheticTargets) {
            relaxedContext.insert(QStringLiteral("target"), target);
            appendRows(listActionsWithContext(capabilityName, relaxedContext));
        }
    };
    appendCapability(QStringLiteral("SearchAction"), false);
    appendCapability(QStringLiteral("QuickAction"), true);
    appendCapability(QStringLiteral("ContextMenu"), true);

    struct Scored {
        int score = 0;
        QVariantMap action;
    };
    QVector<Scored> scored;
    scored.reserve(actions.size());
    for (const QVariant &row : actions) {
        const QVariantMap action = row.toMap();
        FileAction temp;
        temp.id = action.value(QStringLiteral("id")).toString();
        temp.name = action.value(QStringLiteral("name")).toString();
        temp.priority = action.value(QStringLiteral("priority")).toInt();
        temp.keywords = action.value(QStringLiteral("keywords")).toStringList();
        const QVariantMap searchCap = action.value(QStringLiteral("searchAction")).toMap();
        temp.searchAction.intent = searchCap.value(QStringLiteral("intent")).toString();
        const QStringList scopes = searchCap.value(QStringLiteral("scopes")).toStringList();
        for (const QString &scope : scopes) {
            const SearchActionScope parsed = searchActionScopeFromString(scope);
            if (parsed != SearchActionScope::Unknown) {
                temp.searchAction.scopes.insert(parsed);
            }
        }
        const int score = scoreSearchCandidate(temp, parseActionContext(context), query);
        QVariantMap withScore = action;
        withScore.insert(QStringLiteral("score"), score);
        scored.push_back({score, withScore});
    }
    std::sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return QString::localeAwareCompare(a.action.value(QStringLiteral("name")).toString(),
                                           b.action.value(QStringLiteral("name")).toString()) < 0;
    });
    QVariantList out;
    out.reserve(scored.size());
    for (const Scored &entry : scored) {
        out.push_back(entry.action);
    }
    return out;
}

QVariantList ActionRegistry::buildMenuTree(const QVariantList &flatActions) const
{
    struct GroupNode {
        QString path;
        QString label;
        QString parent;
        QVariantList actions;
        QStringList children;
    };

    QHash<QString, GroupNode> groups;
    QVariantList rootActions;
    for (const QVariant &entry : flatActions) {
        const QVariantMap action = entry.toMap();
        const QString group = action.value(QStringLiteral("group")).toString();
        if (group.trimmed().isEmpty()) {
            rootActions.push_back(makeActionNode(action));
            continue;
        }
        const QStringList levels = group.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString path;
        for (const QString &level : levels) {
            path = path.isEmpty() ? level : (path + QLatin1Char('/') + level);
            if (!groups.contains(path)) {
                GroupNode node;
                node.path = path;
                node.label = level;
                node.parent = path.contains(QLatin1Char('/'))
                                  ? path.left(path.lastIndexOf(QLatin1Char('/')))
                                  : QString();
                groups.insert(path, node);
                if (!node.parent.isEmpty()) {
                    GroupNode parent = groups.value(node.parent);
                    if (!parent.children.contains(path)) {
                        parent.children.push_back(path);
                        groups.insert(node.parent, parent);
                    }
                }
            }
        }
        GroupNode leaf = groups.value(path);
        leaf.actions.push_back(makeActionNode(action));
        groups.insert(path, leaf);
    }

    std::function<QVariantMap(const QString &)> serializeGroup = [&](const QString &groupPath) {
        const GroupNode node = groups.value(groupPath);
        QVariantMap out = makeGroupNode(node.path, node.label);
        QVariantList children = out.value(QStringLiteral("children")).toList();
        for (const QString &childPath : node.children) {
            children.push_back(serializeGroup(childPath));
        }
        children += node.actions;
        out.insert(QStringLiteral("children"), children);
        return out;
    };

    QVariantList root;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        if (it->parent.isEmpty()) {
            root.push_back(serializeGroup(it.key()));
        }
    }
    root += rootActions;
    return root;
}

QVariantList ActionRegistry::buildShareSheet(const QVariantList &flatActions) const
{
    return buildMenuTree(flatActions);
}

QVariantList ActionRegistry::buildSearchResults(const QVariantList &flatActions) const
{
    QVariantList out = flatActions;
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap();
        const QVariantMap mb = b.toMap();
        const int sa = ma.value(QStringLiteral("score")).toInt();
        const int sb = mb.value(QStringLiteral("score")).toInt();
        if (sa != sb) {
            return sa > sb;
        }
        return QString::localeAwareCompare(ma.value(QStringLiteral("name")).toString(),
                                           mb.value(QStringLiteral("name")).toString()) < 0;
    });
    return out;
}

} // namespace Slm::Actions
