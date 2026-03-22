#include "filemanagerapi.h"
#include "../../core/actions/slmactionregistry.h"
#include "../../core/actions/framework/slmactionframework.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QProcess>
#include <QDateTime>
#include <QMetaObject>
#include <QUrl>
#include <QSet>

namespace {
constexpr const char kCapabilityService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kCapabilityPath[] = "/org/freedesktop/SLMCapabilities";
constexpr const char kCapabilityInterface[] = "org.freedesktop.SLMCapabilities";

static QString normalizeUri(const QString &value)
{
    const QString v = value.trimmed();
    if (v.isEmpty()) {
        return QString();
    }
    const QUrl url(v);
    if (url.isValid() && !url.scheme().isEmpty()) {
        return url.toString(QUrl::FullyEncoded);
    }
    return QUrl::fromLocalFile(v).toString(QUrl::FullyEncoded);
}

static QString fillExecTemplate(const QString &execTemplate, const QStringList &uris)
{
    QString out = execTemplate;
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
    out.replace(QStringLiteral("%F"), local.join(QLatin1Char(' ')));
    out.replace(QStringLiteral("%f"), uris.isEmpty() ? QString() : QUrl(uris.first()).toLocalFile());
    out.replace(QStringLiteral("%U"), uris.join(QLatin1Char(' ')));
    out.replace(QStringLiteral("%u"), uris.isEmpty() ? QString() : uris.first());
    out.replace(QStringLiteral("%%"), QStringLiteral("%"));
    return out.trimmed();
}

static QVariantList localCapabilityActions(const QString &capability,
                                           const QStringList &normalizedUris,
                                           const QString &target)
{
    Slm::Actions::ActionRegistry localRegistry;
    localRegistry.reload();
    Slm::Actions::Framework::SlmActionFramework framework(&localRegistry);

    QVariantMap context;
    context.insert(QStringLiteral("uris"), normalizedUris);
    context.insert(QStringLiteral("selection_count"), normalizedUris.size());
    const QString t = target.trimmed().toLower();
    if (!t.isEmpty()) {
        context.insert(QStringLiteral("target"), t);
    }
    const QString cap = capability.trimmed().isEmpty()
            ? QStringLiteral("ContextMenu")
            : capability.trimmed();
    return framework.listActions(cap, context);
}

struct MenuNode {
    QString id;
    QString label;
    QString icon;
    QString type; // "submenu" or "action"
    QString actionId;
    QStringList children;
};

static QString sanitizeSegment(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (const QChar ch : value) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-')) {
            out.append(ch.toLower());
        } else {
            out.append(QLatin1Char('_'));
        }
    }
    return out.trimmed();
}

static QVariantMap toNodeMap(const MenuNode &node)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), node.id);
    map.insert(QStringLiteral("label"), node.label);
    map.insert(QStringLiteral("icon"), node.icon);
    map.insert(QStringLiteral("type"), node.type);
    map.insert(QStringLiteral("actionId"), node.actionId);
    map.insert(QStringLiteral("children"), node.children);
    return map;
}

static QVariantMap buildMenuNodeMap(const QVariantList &actions)
{
    QHash<QString, MenuNode> nodes;
    QSet<QString> dedupById;
    QSet<QString> dedupByLabelExec;
    auto ensureSubmenuNode = [&nodes](const QString &nodeId, const QString &label) {
        if (nodes.contains(nodeId)) {
            return;
        }
        MenuNode node;
        node.id = nodeId;
        node.label = label;
        node.type = QStringLiteral("submenu");
        nodes.insert(nodeId, node);
    };
    ensureSubmenuNode(QStringLiteral("root"), QString());

    auto appendChild = [&nodes](const QString &parentId, const QString &childId) {
        auto it = nodes.find(parentId);
        if (it == nodes.end()) {
            return;
        }
        if (!it->children.contains(childId)) {
            it->children.push_back(childId);
        }
    };

    for (const QVariant &row : actions) {
        const QVariantMap action = row.toMap();
        const QString actionId = action.value(QStringLiteral("id")).toString().trimmed();
        const QString name = action.value(QStringLiteral("name")).toString().trimmed();
        const QString group = action.value(QStringLiteral("group")).toString().trimmed();
        const QString exec = action.value(QStringLiteral("exec")).toString().trimmed();
        const QString icon = action.value(QStringLiteral("icon")).toString().trimmed();
        if (actionId.isEmpty() || name.isEmpty()) {
            continue;
        }

        if (dedupById.contains(actionId)) {
            continue;
        }
        dedupById.insert(actionId);

        // Keep only one action when label+exec are identical across providers/files.
        const QString dedupLabelExec = name.toLower() + QLatin1Char('|') + exec.toLower();
        if (dedupByLabelExec.contains(dedupLabelExec)) {
            continue;
        }
        dedupByLabelExec.insert(dedupLabelExec);

        QString parentId = QStringLiteral("root");
        if (!group.isEmpty()) {
            const QStringList levels = group.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            QString path;
            for (const QString &segmentRaw : levels) {
                const QString segment = segmentRaw.trimmed();
                if (segment.isEmpty()) {
                    continue;
                }
                path = path.isEmpty() ? sanitizeSegment(segment)
                                      : (path + QLatin1Char('/') + sanitizeSegment(segment));
                const QString nodeId = QStringLiteral("group:") + path;
                ensureSubmenuNode(nodeId, segment);
                appendChild(parentId, nodeId);
                parentId = nodeId;
            }
        }

        const QString nodeId = QStringLiteral("action:") + actionId;
        MenuNode actionNode;
        actionNode.id = nodeId;
        actionNode.label = name;
        actionNode.icon = icon;
        actionNode.type = QStringLiteral("action");
        actionNode.actionId = actionId;
        nodes.insert(nodeId, actionNode);
        appendChild(parentId, nodeId);
    }

    QVariantMap out;
    for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
        out.insert(it.key(), toNodeMap(it.value()));
    }
    return out;
}

static QVariantList childrenFromNodeMap(const QVariantMap &nodes, const QString &nodeId)
{
    const QString id = nodeId.trimmed().isEmpty() ? QStringLiteral("root") : nodeId.trimmed();
    const QVariantMap parent = nodes.value(id).toMap();
    const QStringList childIds = parent.value(QStringLiteral("children")).toStringList();
    QVariantList rows;
    rows.reserve(childIds.size());
    for (const QString &cid : childIds) {
        const QVariantMap child = nodes.value(cid).toMap();
        if (child.isEmpty()) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), child.value(QStringLiteral("id")).toString());
        row.insert(QStringLiteral("label"), child.value(QStringLiteral("label")).toString());
        row.insert(QStringLiteral("icon"), child.value(QStringLiteral("icon")).toString());
        row.insert(QStringLiteral("type"), child.value(QStringLiteral("type")).toString());
        row.insert(QStringLiteral("actionId"), child.value(QStringLiteral("actionId")).toString());
        row.insert(QStringLiteral("hasChildren"),
                   !child.value(QStringLiteral("children")).toStringList().isEmpty());
        rows.push_back(row);
    }
    return rows;
}

static bool nodeMapHasRootChildren(const QVariantMap &nodes)
{
    const QVariantMap root = nodes.value(QStringLiteral("root")).toMap();
    const QStringList childIds = root.value(QStringLiteral("children")).toStringList();
    return !childIds.isEmpty();
}
} // namespace

SlmContextMenuController::SlmContextMenuController(FileManagerApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
}

QVariantList SlmContextMenuController::rootItems() const
{
    return children(QStringLiteral("root"));
}

QVariantList SlmContextMenuController::children(const QString &nodeId) const
{
    return childrenFromNodeMap(m_nodes, nodeId);
}

QVariantList SlmContextMenuController::menuRootItems() const
{
    return rootItems();
}

QVariantList SlmContextMenuController::menuChildren(const QString &nodeId) const
{
    return children(nodeId);
}

void SlmContextMenuController::invoke(const QString &actionId)
{
    if (!m_api) {
        return;
    }
    QVariantList uris;
    uris.reserve(m_contextUris.size());
    for (const QString &uri : m_contextUris) {
        uris.push_back(uri);
    }
    m_api->startInvokeSlmContextAction(actionId, uris, QString());
}

void SlmContextMenuController::setMenuTree(const QVariantMap &nodes,
                                           const QStringList &contextUris,
                                           const QString &target)
{
    m_nodes = nodes;
    m_contextUris = contextUris;
    m_target = target;
}

void SlmContextMenuController::clear()
{
    m_nodes.clear();
    m_contextUris.clear();
    m_target.clear();
}

QVariantMap FileManagerApi::startSlmContextActions(const QVariantList &uris,
                                                   const QString &target,
                                                   const QString &requestId)
{
    return startSlmCapabilityActions(QStringLiteral("ContextMenu"), uris, target, requestId);
}

QVariantMap FileManagerApi::startSlmCapabilityActions(const QString &capability,
                                                      const QVariantList &uris,
                                                      const QString &target,
                                                      const QString &requestId)
{
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();
    const QString cap = capability.trimmed().isEmpty()
            ? QStringLiteral("ContextMenu")
            : capability.trimmed();
    const bool isContextMenu = (cap.compare(QStringLiteral("ContextMenu"), Qt::CaseInsensitive) == 0);

    QStringList normalizedUris;
    normalizedUris.reserve(uris.size());
    for (const QVariant &entry : uris) {
        const QString uri = normalizeUri(entry.toString());
        if (!uri.isEmpty()) {
            normalizedUris.push_back(uri);
        }
    }

    // Prefill from local registry so UI can render menu entries immediately
    // while DBus resolution is still in-flight.
    const QVariantList prefilledActions = localCapabilityActions(cap, normalizedUris, target);
    if (isContextMenu) {
        m_slmContextMenuNodes = buildMenuNodeMap(prefilledActions);
        updateSlmContextMenuController(m_slmContextMenuNodes, normalizedUris, target);
    }

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityInterface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        const QVariantList actions = prefilledActions;
        if (isContextMenu) {
            m_slmContextMenuNodes = buildMenuNodeMap(actions);
            updateSlmContextMenuController(m_slmContextMenuNodes, normalizedUris, target);
        }
        QMetaObject::invokeMethod(this, [this, rid, cap, actions, isContextMenu]() {
            emit slmCapabilityActionsReady(rid, cap, actions, QString());
            if (isContextMenu) {
                emit slmContextActionsReady(rid, actions, QString());
            }
        }, Qt::QueuedConnection);
        return makeResult(true, QString(),
                          {{QStringLiteral("requestId"), rid},
                           {QStringLiteral("capability"), cap},
                           {QStringLiteral("count"), normalizedUris.size()},
                           {QStringLiteral("actions"), actions},
                           {QStringLiteral("fallback"), QStringLiteral("local-registry")},
                           {QStringLiteral("async"), true}});
    }

    QVariantMap options;
    const QString t = target.trimmed().toLower();
    if (!t.isEmpty()) {
        options.insert(QStringLiteral("target"), t);
    }
    QDBusPendingCall async = iface.asyncCall(QStringLiteral("ListActions"),
                                             cap,
                                             normalizedUris,
                                             options);
    auto *watcher = new QDBusPendingCallWatcher(async, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, rid, cap, prefilledActions, normalizedUris, target, isContextMenu](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariantList> reply = *self;
        QVariantList actions;
        QString error;
        if (reply.isError()) {
            error = reply.error().name();
            if (error.isEmpty()) {
                error = reply.error().message();
            }
            if (error.isEmpty()) {
                error = QStringLiteral("dbus-error");
            }
        } else {
            actions = reply.argumentAt<0>();
        }
        if ((reply.isError() || actions.isEmpty())) {
            const QVariantList fallback = prefilledActions;
            if (!fallback.isEmpty()) {
                actions = fallback;
                error.clear();
            }
        }
        if (isContextMenu) {
            QVariantMap nextNodes = buildMenuNodeMap(actions);
            if (!nodeMapHasRootChildren(nextNodes) && !prefilledActions.isEmpty()) {
                actions = prefilledActions;
                nextNodes = buildMenuNodeMap(actions);
            }
            m_slmContextMenuNodes = nextNodes;
            updateSlmContextMenuController(m_slmContextMenuNodes, normalizedUris, target);
        }
        emit slmCapabilityActionsReady(rid, cap, actions, error);
        if (isContextMenu) {
            emit slmContextActionsReady(rid, actions, error);
        }
        self->deleteLater();
    });

    QVariantMap resultMap = makeResult(true, QString(),
                                       {{QStringLiteral("requestId"), rid},
                                        {QStringLiteral("capability"), cap},
                                        {QStringLiteral("count"), normalizedUris.size()},
                                        {QStringLiteral("actions"), prefilledActions},
                                        {QStringLiteral("async"), true}});
    if (isContextMenu) {
        resultMap.insert(QStringLiteral("menuRootId"), QStringLiteral("root"));
        resultMap.insert(QStringLiteral("menuChildren"),
                         childrenFromNodeMap(m_slmContextMenuNodes, QStringLiteral("root")));
    }
    return resultMap;
}

QVariantMap FileManagerApi::startInvokeSlmCapabilityAction(const QString &capability,
                                                           const QString &actionId,
                                                           const QVariantList &uris,
                                                           const QString &requestId)
{
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();
    const QString cap = capability.trimmed().isEmpty()
            ? QStringLiteral("ContextMenu")
            : capability.trimmed();
    const bool isContextMenu = (cap.compare(QStringLiteral("ContextMenu"), Qt::CaseInsensitive) == 0);
    const QString id = actionId.trimmed();
    if (id.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-action-id"));
    }

    QStringList normalizedUris;
    normalizedUris.reserve(uris.size());
    for (const QVariant &entry : uris) {
        const QString uri = normalizeUri(entry.toString());
        if (!uri.isEmpty()) {
            normalizedUris.push_back(uri);
        }
    }

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityInterface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        Slm::Actions::ActionRegistry localRegistry;
        localRegistry.reload();
        Slm::Actions::Framework::SlmActionFramework framework(&localRegistry);

        const QVariantMap context = {
            {QStringLiteral("uris"), normalizedUris},
            {QStringLiteral("selection_count"), normalizedUris.size()},
        };
        const QVariantMap invokeResult = framework.invokeAction(id, context);
        QVariantMap result = invokeResult;
        result.insert(QStringLiteral("fallback"), QStringLiteral("local-registry"));

        QMetaObject::invokeMethod(this, [this, rid, cap, id, result, isContextMenu]() {
            emit slmCapabilityActionInvoked(rid, cap, id, result);
            if (isContextMenu) {
                emit slmContextActionInvoked(rid, id, result);
            }
        }, Qt::QueuedConnection);
        return makeResult(true, QString(),
                          {{QStringLiteral("requestId"), rid},
                           {QStringLiteral("capability"), cap},
                           {QStringLiteral("actionId"), id},
                           {QStringLiteral("count"), normalizedUris.size()},
                           {QStringLiteral("fallback"), QStringLiteral("local-registry")},
                           {QStringLiteral("async"), true}});
    }

    const QVariantMap options;
    QDBusPendingCall async = iface.asyncCall(QStringLiteral("InvokeAction"),
                                             id,
                                             normalizedUris,
                                             options);
    auto *watcher = new QDBusPendingCallWatcher(async, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, rid, cap, id, isContextMenu](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariantMap> reply = *self;
        QVariantMap result;
        if (reply.isError()) {
            result = makeResult(false,
                                reply.error().name().isEmpty()
                                    ? QStringLiteral("dbus-error")
                                    : reply.error().name());
        } else {
            result = reply.argumentAt<0>();
        }
        emit slmCapabilityActionInvoked(rid, cap, id, result);
        if (isContextMenu) {
            emit slmContextActionInvoked(rid, id, result);
        }
        self->deleteLater();
    });

    return makeResult(true, QString(),
                      {{QStringLiteral("requestId"), rid},
                       {QStringLiteral("capability"), cap},
                       {QStringLiteral("count"), normalizedUris.size()},
                       {QStringLiteral("actionId"), id},
                       {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startInvokeSlmContextAction(const QString &actionId,
                                                        const QVariantList &uris,
                                                        const QString &requestId)
{
    return startInvokeSlmCapabilityAction(QStringLiteral("ContextMenu"),
                                          actionId,
                                          uris,
                                          requestId);
}

QString FileManagerApi::slmContextMenuRootId() const
{
    return QStringLiteral("root");
}

QObject *FileManagerApi::slmContextMenuController() const
{
    return m_slmContextMenuController;
}

QVariantList FileManagerApi::slmContextMenuChildren(const QString &nodeId) const
{
    return childrenFromNodeMap(m_slmContextMenuNodes, nodeId);
}

void FileManagerApi::updateSlmContextMenuController(const QVariantMap &nodes,
                                                    const QStringList &contextUris,
                                                    const QString &target)
{
    if (!m_slmContextMenuController) {
        return;
    }
    m_slmContextMenuController->setMenuTree(nodes, contextUris, target);
}

QVariantMap FileManagerApi::startSlmContextMenuTreeDebug(const QVariantList &uris,
                                                         const QString &target)
{
    QStringList normalizedUris;
    normalizedUris.reserve(uris.size());
    for (const QVariant &entry : uris) {
        const QString uri = normalizeUri(entry.toString());
        if (!uri.isEmpty()) {
            normalizedUris.push_back(uri);
        }
    }

    const QVariantList actions = localCapabilityActions(QStringLiteral("ContextMenu"),
                                                        normalizedUris,
                                                        target);
    m_slmContextMenuNodes = buildMenuNodeMap(actions);
    updateSlmContextMenuController(m_slmContextMenuNodes, normalizedUris, target);
    const QVariantList rootChildren = childrenFromNodeMap(m_slmContextMenuNodes, QStringLiteral("root"));

    return makeResult(true, QString(),
                      {{QStringLiteral("target"), target},
                       {QStringLiteral("count"), normalizedUris.size()},
                       {QStringLiteral("actions"), actions},
                       {QStringLiteral("menuRootId"), QStringLiteral("root")},
                       {QStringLiteral("menuChildren"), rootChildren},
                       {QStringLiteral("nodeCount"), m_slmContextMenuNodes.size()}});
}
QVariantMap FileManagerApi::startResolveSlmDragDropAction(const QVariantList &sourceUris,
                                                         const QString &targetUri,
                                                         const QString &requestId)
{
    const QString rid = requestId.trimmed().isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : requestId.trimmed();

    QStringList normalizedSources;
    normalizedSources.reserve(sourceUris.size());
    for (const QVariant &entry : sourceUris) {
        const QString uri = normalizeUri(entry.toString());
        if (!uri.isEmpty()) {
            normalizedSources.push_back(uri);
        }
    }
    const QString normalizedTarget = normalizeUri(targetUri);

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityInterface),
                         QDBusConnection::sessionBus());

    if (!iface.isValid()) {
        Slm::Actions::ActionRegistry localRegistry;
        localRegistry.reload();
        Slm::Actions::Framework::SlmActionFramework framework(&localRegistry);

        QVariantMap context;
        context.insert(QStringLiteral("uris"), normalizedSources);
        context.insert(QStringLiteral("selection_count"), normalizedSources.size());
        context.insert(QStringLiteral("target"), QStringLiteral("selection"));
        context.insert(QStringLiteral("drag_destination"), QStringLiteral("window"));
        context.insert(QStringLiteral("path"), normalizedTarget);

        const QVariantMap resolved = framework.resolveDefaultAction(QStringLiteral("DragDrop"), context);
        QVariantMap action;
        QString error;
        if (resolved.value(QStringLiteral("ok")).toBool()) {
            action = resolved.value(QStringLiteral("action")).toMap();
        } else {
            error = resolved.value(QStringLiteral("error")).toString();
        }

        QMetaObject::invokeMethod(this, [this, rid, action, error]() {
            emit slmDragDropActionResolved(rid, action, error);
        }, Qt::QueuedConnection);

        return makeResult(true, QString(),
                          {{QStringLiteral("requestId"), rid},
                           {QStringLiteral("async"), true},
                           {QStringLiteral("fallback"), QStringLiteral("local-registry")}});
    }

    QVariantMap options;
    options.insert(QStringLiteral("target"), QStringLiteral("selection"));
    options.insert(QStringLiteral("drag_destination"), QStringLiteral("window"));
    options.insert(QStringLiteral("path"), normalizedTarget);

    QDBusPendingCall async = iface.asyncCall(QStringLiteral("ResolveDefaultAction"),
                                             QStringLiteral("DragDrop"),
                                             normalizedSources,
                                             options);

    auto *watcher = new QDBusPendingCallWatcher(async, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, rid](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariantMap> reply = *self;
        QVariantMap result;
        QVariantMap action;
        QString error;

        if (reply.isError()) {
            error = reply.error().name();
        } else {
            result = reply.argumentAt<0>();
            if (result.value(QStringLiteral("ok")).toBool()) {
                action = result.value(QStringLiteral("action")).toMap();
            } else {
                error = result.value(QStringLiteral("error")).toString();
            }
        }
        emit slmDragDropActionResolved(rid, action, error);
        self->deleteLater();
    });

    return makeResult(true, QString(),
                      {{QStringLiteral("requestId"), rid},
                       {QStringLiteral("async"), true}});
}
