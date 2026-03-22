#pragma once

#include "slmactioncondition.h"
#include "slmactiondiscovery.h"
#include "slmactiontypes.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class AppExecutionGate;

namespace Slm::Actions {

class ActionRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ActionRegistry(QObject *parent = nullptr);
    void setExecutionGate(AppExecutionGate *gate);

    void setScanDirectories(const QStringList &directories);
    QStringList scanDirectories() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE QVariantList listActions(const QString &capability,
                                         const QStringList &uris,
                                         const QVariantMap &options = QVariantMap());
    Q_INVOKABLE QVariantList listActionsWithContext(const QString &capability,
                                                    const QVariantMap &context);
    Q_INVOKABLE QVariantMap invokeAction(const QString &actionId,
                                         const QStringList &uris,
                                         const QVariantMap &options = QVariantMap()) const;
    Q_INVOKABLE QVariantMap invokeActionWithContext(const QString &actionId,
                                                    const QVariantMap &context) const;
    Q_INVOKABLE QVariantMap resolveDefaultAction(const QString &capability,
                                                 const QVariantMap &context);
    Q_INVOKABLE QVariantList searchActions(const QString &query,
                                           const QVariantMap &context);
    Q_INVOKABLE QVariantList buildMenuTree(const QVariantList &flatActions) const;
    Q_INVOKABLE QVariantList buildShareSheet(const QVariantList &flatActions) const;
    Q_INVOKABLE QVariantList buildSearchResults(const QVariantList &flatActions) const;

signals:
    void indexingStarted();
    void indexingFinished();
    void providerRegistered(const QString &providerId);
    void registryUpdated();

private:
    static ActionContext parseActionContext(const QVariantMap &context);
    static QVariantMap buildContextMap(const QStringList &uris, const QVariantMap &options);
    static ActionEvalContext buildContext(const QStringList &uris, const QVariantMap &options);
    static ActionEvalContext toEvalContext(const ActionContext &context);
    static bool targetAllowed(const FileAction &action, const ActionEvalContext &ctx);
    static bool targetAllowed(const QSet<Target> &targets, const ActionEvalContext &ctx);
    static bool capabilityContextAllowed(const FileAction &action,
                                         Capability requested,
                                         const ActionContext &context,
                                         const ActionEvalContext &evalCtx);
    static QString fillExec(const QString &execTemplate, const QStringList &uris);
    static int scoreSearchCandidate(const FileAction &action,
                                    const ActionContext &context,
                                    const QString &query);

    ActionDiscoveryScanner m_scanner;
    ActionConditionParser m_conditionParser;
    ActionConditionEvaluator m_conditionEvaluator;
    AppExecutionGate *m_executionGate = nullptr;
};

} // namespace Slm::Actions
