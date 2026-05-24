#include "actiondaemon.h"

#include <QCollator>
#include <QDateTime>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QUuid>
#include <algorithm>

namespace Slm::Actiond {

namespace {
constexpr const char kFallbackProviderId[] = "org.slm.Actiond.fallback";
constexpr const char kFallbackContextId[] = "fallback.system";
constexpr const char kDesktopViewProviderId[] = "org.slm.FileManager.desktopview";
constexpr const char kFileOpsService[] = "org.slm.Desktop.FileOperations";
constexpr const char kFileOpsPath[] = "/org/slm/Desktop/FileOperations";
constexpr const char kFileOpsIface[] = "org.slm.Desktop.FileOperations";
constexpr const char kFileManagerService[] = "org.slm.Desktop.FileManager1";
constexpr const char kFileManagerPath[] = "/org/slm/Desktop/FileManager1";
constexpr const char kFileManagerIface[] = "org.slm.Desktop.FileManager1";
constexpr const char kFreedesktopFmService[] = "org.freedesktop.FileManager1";
constexpr const char kFreedesktopFmPath[] = "/org/freedesktop/FileManager1";
constexpr const char kFreedesktopFmIface[] = "org.freedesktop.FileManager1";

QString normalized(const QString &v)
{
    return v.trimmed();
}

bool boolOrDefault(const QVariantMap &m, const QString &k, bool d)
{
    return m.value(k, d).toBool();
}
} // namespace

ActionDaemon::ActionDaemon(QObject *parent)
    : QObject(parent)
{
    ensureFallbackProvider();
    recomputeActiveContext();
}

bool ActionDaemon::registerProvider(const QString &caller,
                                    const QString &providerId,
                                    const QString &appId,
                                    const QVariantMap &capabilities,
                                    QString *error)
{
    const QString pid = normalized(providerId);
    if (pid.isEmpty()) {
        if (error) *error = QStringLiteral("invalid_provider_id");
        return false;
    }
    if (m_providers.contains(pid) && m_providers.value(pid).ownerBus != caller && !caller.isEmpty()) {
        if (error) *error = QStringLiteral("provider_already_owned");
        return false;
    }

    ProviderInfo info{pid, normalized(appId), capabilities, caller};
    m_providers.insert(pid, info);
    emit providerRegistered(pid, info.appId);
    return true;
}

bool ActionDaemon::unregisterProvider(const QString &caller, const QString &providerId, QString *error)
{
    const QString pid = normalized(providerId);
    if (!checkProviderOwnership(caller, pid, error)) {
        return false;
    }

    m_providers.remove(pid);
    QStringList removedContexts;
    for (auto it = m_contexts.begin(); it != m_contexts.end();) {
        if (it.value().value(QStringLiteral("provider_id")).toString() == pid) {
            removedContexts << it.key();
            it = m_contexts.erase(it);
        } else {
            ++it;
        }
    }
    for (const QString &ctx : removedContexts) {
        m_actionsByContext.remove(ctx);
        emit actionsChanged(ctx, QVariantList());
    }

    emit providerUnregistered(pid);
    recomputeActiveContext();
    emit searchIndexChanged();
    return true;
}

bool ActionDaemon::registerContext(const QString &caller, const QString &providerId, const QVariantMap &context, QString *error)
{
    if (!checkProviderOwnership(caller, normalized(providerId), error)) {
        return false;
    }
    const QVariantMap normalizedContext = normalizeContext(normalized(providerId), context);
    const QString contextId = normalizedContext.value(QStringLiteral("context_id")).toString();
    if (contextId.isEmpty()) {
        if (error) *error = QStringLiteral("invalid_context_id");
        return false;
    }
    m_contexts.insert(contextId, normalizedContext);
    emit contextChanged(contextId, normalizedContext);
    recomputeActiveContext();
    return true;
}

bool ActionDaemon::updateContext(const QString &caller, const QString &providerId, const QVariantMap &context, QString *error)
{
    return registerContext(caller, providerId, context, error);
}

bool ActionDaemon::activateContext(const QString &contextId, QString *error)
{
    const QString cid = normalized(contextId);
    if (!m_contexts.contains(cid)) {
        if (error) *error = QStringLiteral("context_not_found");
        return false;
    }
    QVariantMap ctx = m_contexts.value(cid);
    ctx.insert(QStringLiteral("active"), true);
    m_contexts.insert(cid, ctx);
    emit contextChanged(cid, ctx);
    recomputeActiveContext();
    return true;
}

bool ActionDaemon::deactivateContext(const QString &contextId, QString *error)
{
    const QString cid = normalized(contextId);
    if (!m_contexts.contains(cid)) {
        if (error) *error = QStringLiteral("context_not_found");
        return false;
    }
    QVariantMap ctx = m_contexts.value(cid);
    ctx.insert(QStringLiteral("active"), false);
    m_contexts.insert(cid, ctx);
    emit contextChanged(cid, ctx);
    recomputeActiveContext();
    return true;
}

bool ActionDaemon::setActions(const QString &caller,
                              const QString &providerId,
                              const QString &contextId,
                              const QVariantList &actions,
                              QString *error)
{
    if (!checkProviderOwnership(caller, normalized(providerId), error)) {
        return false;
    }
    const QString cid = resolveContextId(contextId);
    if (!m_contexts.contains(cid)) {
        if (error) *error = QStringLiteral("context_not_found");
        return false;
    }

    QHash<QString, QVariantMap> out;
    for (const QVariant &v : actions) {
        const QVariantMap action = normalizeAction(normalized(providerId), cid, v.toMap());
        const QString aid = action.value(QStringLiteral("id")).toString();
        if (!aid.isEmpty()) {
            out.insert(aid, action);
        }
    }
    m_actionsByContext.insert(cid, out);

    const QVariantList sorted = getActions(cid);
    emit actionsChanged(cid, sorted);
    emit menuModelChanged(cid, getMenuModel(cid));
    emit searchIndexChanged();
    return true;
}

bool ActionDaemon::updateAction(const QString &caller,
                                const QString &providerId,
                                const QString &contextId,
                                const QString &actionId,
                                const QVariantMap &patch,
                                QString *error)
{
    if (!checkProviderOwnership(caller, normalized(providerId), error)) {
        return false;
    }
    const QString cid = resolveContextId(contextId);
    const QString aid = normalized(actionId);
    if (!m_actionsByContext.contains(cid) || !m_actionsByContext.value(cid).contains(aid)) {
        if (error) *error = QStringLiteral("action_not_found");
        return false;
    }

    QVariantMap action = m_actionsByContext.value(cid).value(aid);
    for (auto it = patch.begin(); it != patch.end(); ++it) {
        action.insert(it.key(), it.value());
    }
    m_actionsByContext[cid].insert(aid, action);
    emit actionStateChanged(cid, aid, action);
    emit actionsChanged(cid, getActions(cid));
    emit menuModelChanged(cid, getMenuModel(cid));
    emit searchIndexChanged();
    return true;
}

bool ActionDaemon::removeAction(const QString &caller,
                                const QString &providerId,
                                const QString &contextId,
                                const QString &actionId,
                                QString *error)
{
    if (!checkProviderOwnership(caller, normalized(providerId), error)) {
        return false;
    }
    const QString cid = resolveContextId(contextId);
    const QString aid = normalized(actionId);
    if (!m_actionsByContext.contains(cid) || !m_actionsByContext.value(cid).contains(aid)) {
        if (error) *error = QStringLiteral("action_not_found");
        return false;
    }
    m_actionsByContext[cid].remove(aid);
    emit actionsChanged(cid, getActions(cid));
    emit menuModelChanged(cid, getMenuModel(cid));
    emit searchIndexChanged();
    return true;
}

QVariantMap ActionDaemon::getActiveContext() const
{
    if (m_contexts.contains(m_activeContextId)) {
        return m_contexts.value(m_activeContextId);
    }
    return m_contexts.value(QString::fromLatin1(kFallbackContextId));
}

QVariantList ActionDaemon::getProviders() const
{
    QVariantList out;
    for (const ProviderInfo &p : m_providers) {
        out << QVariantMap{
            {QStringLiteral("provider_id"), p.providerId},
            {QStringLiteral("app_id"), p.appId},
            {QStringLiteral("capabilities"), p.capabilities},
            {QStringLiteral("owner_bus"), p.ownerBus},
        };
    }
    return out;
}

QVariantList ActionDaemon::getContexts() const
{
    QVariantList out;
    for (const QVariantMap &ctx : m_contexts) {
        out << ctx;
    }
    return out;
}

QVariantList ActionDaemon::getActions(const QString &contextId) const
{
    const QString cid = resolveContextId(contextId);
    QVariantList out;
    const auto byId = m_actionsByContext.value(cid);
    for (const QVariantMap &action : byId) {
        if (!boolOrDefault(action, QStringLiteral("visible"), true)) {
            continue;
        }
        out << action;
    }
    return sortedActions(out);
}

QVariantList ActionDaemon::getMenuModel(const QString &contextId) const
{
    const QVariantList actions = getActions(contextId);
    QHash<QString, QVariantList> grouped;
    for (const QVariant &v : actions) {
        const QVariantMap a = v.toMap();
        const QString section = a.value(QStringLiteral("section")).toString().trimmed().isEmpty()
            ? QStringLiteral("Actions")
            : a.value(QStringLiteral("section")).toString().trimmed();
        QVariantMap item;
        item.insert(QStringLiteral("label"), a.value(QStringLiteral("label")));
        item.insert(QStringLiteral("action_id"), a.value(QStringLiteral("id")));
        item.insert(QStringLiteral("enabled"), a.value(QStringLiteral("enabled"), true));
        item.insert(QStringLiteral("checkable"), a.value(QStringLiteral("checkable"), false));
        item.insert(QStringLiteral("checked"), a.value(QStringLiteral("checked"), false));
        item.insert(QStringLiteral("kind"), a.value(QStringLiteral("kind"), QStringLiteral("command")));
        grouped[section] << item;
    }

    QVariantList menu;
    QStringList keys = grouped.keys();
    keys.sort(Qt::CaseInsensitive);
    for (const QString &k : keys) {
        QVariantMap section;
        section.insert(QStringLiteral("title"), k);
        section.insert(QStringLiteral("items"), grouped.value(k));
        menu << section;
    }
    return menu;
}

QVariantList ActionDaemon::searchActions(const QString &query, const QVariantMap &contextFilter) const
{
    const QString q = query.trimmed().toLower();
    const QString requestedContext = contextFilter.value(QStringLiteral("context_id")).toString().trimmed();
    QVariantList out;

    auto collect = [&](const QString &cid, int scoreBoost) {
        for (const QVariant &v : getActions(cid)) {
            QVariantMap a = v.toMap();
            const QString hay = (a.value(QStringLiteral("label")).toString() + QLatin1Char(' ')
                                 + a.value(QStringLiteral("description")).toString()).toLower();
            if (!q.isEmpty() && !hay.contains(q)) {
                continue;
            }
            a.insert(QStringLiteral("context_id"), cid);
            a.insert(QStringLiteral("score"), scoreBoost + a.value(QStringLiteral("priority"), 0).toInt());
            if (!a.value(QStringLiteral("enabled"), true).toBool()) {
                a.insert(QStringLiteral("disabled_reason"), QStringLiteral("not_available_in_current_state"));
            }
            out << a;
        }
    };

    if (!requestedContext.isEmpty()) {
        collect(resolveContextId(requestedContext), 1000);
    } else {
        collect(m_activeContextId, 5000);
        if (m_activeContextId != QString::fromLatin1(kFallbackContextId)) {
            collect(QString::fromLatin1(kFallbackContextId), 500);
        }
    }

    std::sort(out.begin(), out.end(), [](const QVariant &lhs, const QVariant &rhs) {
        return lhs.toMap().value(QStringLiteral("score")).toInt() > rhs.toMap().value(QStringLiteral("score")).toInt();
    });
    return out;
}

QVariantList ActionDaemon::getQuickActions(const QString &appId) const
{
    QVariantList out;
    for (const QString &contextId : m_contexts.keys()) {
        const QVariantMap ctx = m_contexts.value(contextId);
        if (ctx.value(QStringLiteral("app_id")).toString() != appId) {
            continue;
        }
        const QVariantList actions = getActions(contextId);
        for (const QVariant &v : actions) {
            const QVariantMap a = v.toMap();
            const QString role = a.value(QStringLiteral("role")).toString();
            if (role.startsWith(QStringLiteral("app."))
                || role == QLatin1String("window.close")
                || role == QLatin1String("window.minimize")) {
                out << a;
            }
        }
    }
    if (out.isEmpty()) {
        out = getActions(QString::fromLatin1(kFallbackContextId));
    }
    return sortedActions(out);
}

QVariantMap ActionDaemon::invokeAction(const QString &actionId,
                                       const QString &contextId,
                                       const QVariantMap &parameters,
                                       bool fromFrontend,
                                       QString *error)
{
    const QString cid = resolveContextId(contextId);
    const QString aid = normalized(actionId);
    const auto byAction = m_actionsByContext.value(cid);
    if (!byAction.contains(aid)) {
        if (error) *error = QStringLiteral("action_not_found");
        return QVariantMap{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("action_not_found")}};
    }

    const QVariantMap action = byAction.value(aid);
    if (!action.value(QStringLiteral("enabled"), true).toBool()) {
        if (error) *error = QStringLiteral("action_disabled");
        return QVariantMap{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("action_disabled")}};
    }

    if (isDestructiveAction(action)) {
        const QString gesture = parameters.value(QStringLiteral("user_gesture_token")).toString().trimmed();
        if (!fromFrontend || gesture.isEmpty()) {
            if (error) *error = QStringLiteral("user_gesture_required");
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), QStringLiteral("user_gesture_required")}};
        }
    }

    QVariantMap result{
        {QStringLiteral("ok"), true},
        {QStringLiteral("action_id"), aid},
        {QStringLiteral("context_id"), cid},
        {QStringLiteral("provider_id"), action.value(QStringLiteral("provider_id"))},
        {QStringLiteral("timestamp_ms"), QDateTime::currentMSecsSinceEpoch()},
        {QStringLiteral("parameters"), parameters},
    };

    const QVariantMap context = m_contexts.value(cid);
    if (action.value(QStringLiteral("provider_id")).toString() == QLatin1String(kDesktopViewProviderId)) {
        const QVariantMap execution = executeDesktopViewAction(action, context, parameters);
        if (!execution.value(QStringLiteral("ok"), false).toBool()) {
            if (error) {
                *error = execution.value(QStringLiteral("error"), QStringLiteral("invoke_failed")).toString();
            }
            emit actionInvoked(aid, cid, execution);
            return execution;
        }
        for (auto it = execution.begin(); it != execution.end(); ++it) {
            result.insert(it.key(), it.value());
        }
    }

    qInfo().noquote() << "[slm-actiond] invoke" << aid << "context=" << cid
                      << "provider=" << action.value(QStringLiteral("provider_id")).toString();
    emit actionInvoked(aid, cid, result);
    return result;
}

bool ActionDaemon::checkProviderOwnership(const QString &caller, const QString &providerId, QString *error) const
{
    if (!m_providers.contains(providerId)) {
        if (error) *error = QStringLiteral("provider_not_found");
        return false;
    }
    if (!caller.isEmpty() && !m_providers.value(providerId).ownerBus.isEmpty()
        && m_providers.value(providerId).ownerBus != caller) {
        if (error) *error = QStringLiteral("provider_ownership_violation");
        return false;
    }
    return true;
}

QVariantMap ActionDaemon::normalizeContext(const QString &providerId, const QVariantMap &context) const
{
    QVariantMap out = context;
    out.insert(QStringLiteral("provider_id"), providerId);
    out.insert(QStringLiteral("context_id"), normalized(out.value(QStringLiteral("context_id")).toString()));
    out.insert(QStringLiteral("type"), normalized(out.value(QStringLiteral("type")).toString()));
    out.insert(QStringLiteral("priority"), out.value(QStringLiteral("priority"), 0).toInt());
    out.insert(QStringLiteral("active"), out.value(QStringLiteral("active"), true).toBool());
    return out;
}

QVariantMap ActionDaemon::normalizeAction(const QString &providerId, const QString &contextId, const QVariantMap &action) const
{
    QVariantMap out = action;
    out.insert(QStringLiteral("provider_id"), providerId);
    out.insert(QStringLiteral("context_id"), contextId);
    out.insert(QStringLiteral("id"), normalized(out.value(QStringLiteral("id")).toString()));
    out.insert(QStringLiteral("label"), normalized(out.value(QStringLiteral("label")).toString()));
    out.insert(QStringLiteral("enabled"), out.value(QStringLiteral("enabled"), true).toBool());
    out.insert(QStringLiteral("visible"), out.value(QStringLiteral("visible"), true).toBool());
    out.insert(QStringLiteral("priority"), out.value(QStringLiteral("priority"), 0).toInt());
    out.insert(QStringLiteral("kind"), out.value(QStringLiteral("kind"), QStringLiteral("command")));
    if (!out.contains(QStringLiteral("section"))) {
        out.insert(QStringLiteral("section"), QStringLiteral("Actions"));
    }
    return out;
}

void ActionDaemon::ensureFallbackProvider()
{
    registerProvider(QStringLiteral("internal"),
                     QString::fromLatin1(kFallbackProviderId),
                     QStringLiteral("org.slm.System"),
                     QVariantMap{{QStringLiteral("fallback"), true}});

    registerContext(QStringLiteral("internal"),
                    QString::fromLatin1(kFallbackProviderId),
                    QVariantMap{{QStringLiteral("context_id"), QString::fromLatin1(kFallbackContextId)},
                                {QStringLiteral("type"), QStringLiteral("fallback")},
                                {QStringLiteral("priority"), 0},
                                {QStringLiteral("active"), true},
                                {QStringLiteral("app_id"), QStringLiteral("org.slm.System")}});

    QVariantList fallbackActions{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("window.minimize")},
                    {QStringLiteral("label"), QStringLiteral("Minimize")},
                    {QStringLiteral("role"), QStringLiteral("window.minimize")},
                    {QStringLiteral("kind"), QStringLiteral("window-operation")},
                    {QStringLiteral("section"), QStringLiteral("Window")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("window.maximize")},
                    {QStringLiteral("label"), QStringLiteral("Maximize")},
                    {QStringLiteral("role"), QStringLiteral("window.maximize")},
                    {QStringLiteral("kind"), QStringLiteral("window-operation")},
                    {QStringLiteral("section"), QStringLiteral("Window")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("window.close")},
                    {QStringLiteral("label"), QStringLiteral("Close")},
                    {QStringLiteral("role"), QStringLiteral("window.close")},
                    {QStringLiteral("kind"), QStringLiteral("destructive")},
                    {QStringLiteral("destructive"), true},
                    {QStringLiteral("section"), QStringLiteral("Window")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("app.quit")},
                    {QStringLiteral("label"), QStringLiteral("Quit")},
                    {QStringLiteral("role"), QStringLiteral("app.quit")},
                    {QStringLiteral("kind"), QStringLiteral("destructive")},
                    {QStringLiteral("destructive"), true},
                    {QStringLiteral("section"), QStringLiteral("App")}},
    };

    setActions(QStringLiteral("internal"),
               QString::fromLatin1(kFallbackProviderId),
               QString::fromLatin1(kFallbackContextId),
               fallbackActions);
}

void ActionDaemon::recomputeActiveContext()
{
    QString bestId = QString::fromLatin1(kFallbackContextId);
    int bestRank = -1;

    for (auto it = m_contexts.begin(); it != m_contexts.end(); ++it) {
        const QVariantMap ctx = it.value();
        if (!ctx.value(QStringLiteral("active"), false).toBool()) {
            continue;
        }
        const int rank = contextRank(ctx);
        if (rank > bestRank) {
            bestRank = rank;
            bestId = it.key();
        }
    }

    if (bestId != m_activeContextId) {
        m_activeContextId = bestId;
        emit activeContextChanged(getActiveContext());
        emit menuModelChanged(m_activeContextId, getMenuModel(m_activeContextId));
    }
}

int ActionDaemon::contextRank(const QVariantMap &ctx) const
{
    const QString type = ctx.value(QStringLiteral("type")).toString();
    int base = 0;
    if (type == QLatin1String("modal-dialog")) base = 500;
    else if (type == QLatin1String("focused-window")) base = 400;
    else if (type == QLatin1String("desktop-selection")) base = 300;
    else if (type == QLatin1String("desktopview")) base = 200;
    else if (type == QLatin1String("system")) base = 100;
    else if (type == QLatin1String("fallback")) base = 1;
    return base + ctx.value(QStringLiteral("priority"), 0).toInt();
}

QString ActionDaemon::resolveContextId(const QString &contextId) const
{
    const QString cid = normalized(contextId);
    if (!cid.isEmpty()) {
        return cid;
    }
    return m_activeContextId.isEmpty() ? QString::fromLatin1(kFallbackContextId) : m_activeContextId;
}

QVariantList ActionDaemon::sortedActions(const QVariantList &actions) const
{
    QVariantList out = actions;
    std::sort(out.begin(), out.end(), [](const QVariant &lhs, const QVariant &rhs) {
        const QVariantMap a = lhs.toMap();
        const QVariantMap b = rhs.toMap();
        const int pa = a.value(QStringLiteral("priority"), 0).toInt();
        const int pb = b.value(QStringLiteral("priority"), 0).toInt();
        if (pa != pb) {
            return pa > pb;
        }
        return a.value(QStringLiteral("label")).toString().toLower()
             < b.value(QStringLiteral("label")).toString().toLower();
    });
    return out;
}

bool ActionDaemon::isDestructiveAction(const QVariantMap &action) const
{
    if (action.value(QStringLiteral("destructive"), false).toBool()) {
        return true;
    }
    const QString kind = action.value(QStringLiteral("kind")).toString();
    return kind == QLatin1String("destructive");
}

QVariantMap ActionDaemon::executeDesktopViewAction(const QVariantMap &action,
                                                   const QVariantMap &context,
                                                   const QVariantMap &) const
{
    const QString actionId = action.value(QStringLiteral("id")).toString();
    const QUrl locationUrl(context.value(QStringLiteral("location")).toString());
    QString baseDir = locationUrl.isLocalFile() ? locationUrl.toLocalFile() : QString();
    if (baseDir.trimmed().isEmpty()) {
        baseDir = QDir::homePath() + QStringLiteral("/Desktop");
    }

    if (actionId == QLatin1String("desktop.new-folder")) {
        const QString path = uniqueChildPath(baseDir, QStringLiteral("New Folder"));
        if (QDir().mkpath(path)) {
            return QVariantMap{{QStringLiteral("ok"), true},
                               {QStringLiteral("operation"), QStringLiteral("mkdir")},
                               {QStringLiteral("path"), path}};
        }
        return QVariantMap{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("mkdir_failed")},
                           {QStringLiteral("path"), path}};
    }

    if (actionId == QLatin1String("desktop.new-document")) {
        const QString path = uniqueChildPath(baseDir, QStringLiteral("New Document"), QStringLiteral(".txt"));
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.close();
            return QVariantMap{{QStringLiteral("ok"), true},
                               {QStringLiteral("operation"), QStringLiteral("touch")},
                               {QStringLiteral("path"), path}};
        }
        return QVariantMap{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("create_file_failed")},
                           {QStringLiteral("path"), path}};
    }

    if (actionId == QLatin1String("file.move-to-trash")) {
        const QStringList uris = context.value(QStringLiteral("selection")).toStringList();
        if (uris.isEmpty()) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), QStringLiteral("empty_selection")}};
        }
        QDBusInterface fileOps(QString::fromLatin1(kFileOpsService),
                               QString::fromLatin1(kFileOpsPath),
                               QString::fromLatin1(kFileOpsIface),
                               QDBusConnection::sessionBus());
        if (!fileOps.isValid()) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), QStringLiteral("fileops_unavailable")}};
        }
        const QDBusReply<QString> reply = fileOps.call(QStringLiteral("Trash"), uris);
        if (!reply.isValid() || reply.value().trimmed().isEmpty()) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), QStringLiteral("trash_failed")}};
        }
        return QVariantMap{{QStringLiteral("ok"), true},
                           {QStringLiteral("operation"), QStringLiteral("trash")},
                           {QStringLiteral("job_id"), reply.value()}};
    }

    if (actionId == QLatin1String("edit.select-all")
        ) {
        return QVariantMap{{QStringLiteral("ok"), true},
                           {QStringLiteral("operation"), QStringLiteral("delegated-ui")}};
    }

    if (actionId == QLatin1String("desktop.open-in-filemanager")) {
        QString error;
        if (!openPathInFileManager(baseDir, &error)) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), error.isEmpty()
                                                            ? QStringLiteral("open_path_failed")
                                                            : error}};
        }
        return QVariantMap{{QStringLiteral("ok"), true},
                           {QStringLiteral("operation"), QStringLiteral("open-filemanager")},
                           {QStringLiteral("path"), baseDir}};
    }

    if (actionId == QLatin1String("file.open")) {
        const QStringList uris = context.value(QStringLiteral("selection")).toStringList();
        if (uris.isEmpty()) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), QStringLiteral("empty_selection")}};
        }
        const QUrl u(uris.first());
        const QString path = u.isLocalFile() ? u.toLocalFile() : uris.first();
        QString error;
        if (!openPathInFileManager(path, &error)) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), error.isEmpty()
                                                            ? QStringLiteral("open_path_failed")
                                                            : error}};
        }
        return QVariantMap{{QStringLiteral("ok"), true},
                           {QStringLiteral("operation"), QStringLiteral("open-path")},
                           {QStringLiteral("path"), path}};
    }

    if (actionId == QLatin1String("file.get-info")) {
        const QStringList uris = context.value(QStringLiteral("selection")).toStringList();
        if (uris.isEmpty()) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), QStringLiteral("empty_selection")}};
        }
        QString error;
        if (!revealUriInFileManager(uris.first(), &error)) {
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), error.isEmpty()
                                                            ? QStringLiteral("reveal_failed")
                                                            : error}};
        }
        return QVariantMap{{QStringLiteral("ok"), true},
                           {QStringLiteral("operation"), QStringLiteral("reveal-item")},
                           {QStringLiteral("uri"), uris.first()}};
    }

    if (actionId == QLatin1String("file.rename") || actionId == QLatin1String("edit.paste")) {
        return QVariantMap{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("provider_ui_required")}};
    }

    return QVariantMap{{QStringLiteral("ok"), true},
                       {QStringLiteral("operation"), QStringLiteral("noop")}};
}

QString ActionDaemon::uniqueChildPath(const QString &baseDir,
                                      const QString &baseName,
                                      const QString &suffix) const
{
    QString candidate = QDir(baseDir).filePath(baseName + suffix);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }
    for (int i = 2; i < 1000; ++i) {
        candidate = QDir(baseDir).filePath(baseName + QStringLiteral(" %1").arg(i) + suffix);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QDir(baseDir).filePath(baseName + QStringLiteral(" ") + QString::number(QDateTime::currentSecsSinceEpoch()) + suffix);
}

bool ActionDaemon::openPathInFileManager(const QString &path, QString *error) const
{
    const QString reqId = QUuid::createUuid().toString(QUuid::Id128).left(8);
    QDBusInterface slmFm(QString::fromLatin1(kFileManagerService),
                         QString::fromLatin1(kFileManagerPath),
                         QString::fromLatin1(kFileManagerIface),
                         QDBusConnection::sessionBus());
    if (slmFm.isValid()) {
        const QDBusReply<QVariantMap> reply = slmFm.call(QStringLiteral("OpenPath"), path, reqId);
        if (reply.isValid()) {
            return true;
        }
    }

    const QString uri = QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    QDBusInterface freedesktopFm(QString::fromLatin1(kFreedesktopFmService),
                                 QString::fromLatin1(kFreedesktopFmPath),
                                 QString::fromLatin1(kFreedesktopFmIface),
                                 QDBusConnection::sessionBus());
    if (!freedesktopFm.isValid()) {
        if (error) *error = QStringLiteral("filemanager_unavailable");
        return false;
    }
    QDBusMessage msg = freedesktopFm.call(QStringLiteral("ShowFolders"),
                                          QStringList{uri},
                                          QString());
    if (msg.type() == QDBusMessage::ErrorMessage) {
        if (error) *error = QStringLiteral("freedesktop_showfolders_failed");
        return false;
    }
    return true;
}

bool ActionDaemon::revealUriInFileManager(const QString &uri, QString *error) const
{
    const QUrl qurl(uri);
    const QString localPath = qurl.isLocalFile() ? qurl.toLocalFile() : uri;
    const QString reqId = QUuid::createUuid().toString(QUuid::Id128).left(8);
    QDBusInterface slmFm(QString::fromLatin1(kFileManagerService),
                         QString::fromLatin1(kFileManagerPath),
                         QString::fromLatin1(kFileManagerIface),
                         QDBusConnection::sessionBus());
    if (slmFm.isValid()) {
        const QDBusReply<QVariantMap> revealReply = slmFm.call(QStringLiteral("RevealItem"), localPath, reqId);
        if (revealReply.isValid()) {
            return true;
        }
    }

    QDBusInterface freedesktopFm(QString::fromLatin1(kFreedesktopFmService),
                                 QString::fromLatin1(kFreedesktopFmPath),
                                 QString::fromLatin1(kFreedesktopFmIface),
                                 QDBusConnection::sessionBus());
    if (!freedesktopFm.isValid()) {
        if (error) *error = QStringLiteral("filemanager_unavailable");
        return false;
    }
    const QString resolvedUri = qurl.isValid() && !qurl.scheme().isEmpty()
        ? qurl.toString(QUrl::FullyEncoded)
        : QUrl::fromLocalFile(localPath).toString(QUrl::FullyEncoded);
    QDBusMessage msg = freedesktopFm.call(QStringLiteral("ShowItems"),
                                          QStringList{resolvedUri},
                                          QString());
    if (msg.type() == QDBusMessage::ErrorMessage) {
        if (error) *error = QStringLiteral("freedesktop_showitems_failed");
        return false;
    }
    return true;
}

} // namespace Slm::Actiond
