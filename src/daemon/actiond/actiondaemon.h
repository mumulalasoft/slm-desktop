#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::Actiond {

class ActionDaemon : public QObject
{
    Q_OBJECT

public:
    explicit ActionDaemon(QObject *parent = nullptr);

    bool registerProvider(const QString &caller,
                          const QString &providerId,
                          const QString &appId,
                          const QVariantMap &capabilities,
                          QString *error = nullptr);
    bool unregisterProvider(const QString &caller, const QString &providerId, QString *error = nullptr);

    bool registerContext(const QString &caller, const QString &providerId, const QVariantMap &context, QString *error = nullptr);
    bool updateContext(const QString &caller, const QString &providerId, const QVariantMap &context, QString *error = nullptr);
    bool activateContext(const QString &contextId, QString *error = nullptr);
    bool deactivateContext(const QString &contextId, QString *error = nullptr);

    bool setActions(const QString &caller,
                    const QString &providerId,
                    const QString &contextId,
                    const QVariantList &actions,
                    QString *error = nullptr);
    bool updateAction(const QString &caller,
                      const QString &providerId,
                      const QString &contextId,
                      const QString &actionId,
                      const QVariantMap &patch,
                      QString *error = nullptr);
    bool removeAction(const QString &caller,
                      const QString &providerId,
                      const QString &contextId,
                      const QString &actionId,
                      QString *error = nullptr);

    QVariantMap getActiveContext() const;
    QVariantList getProviders() const;
    QVariantList getContexts() const;
    QVariantList getActions(const QString &contextId) const;
    QVariantList getMenuModel(const QString &contextId) const;
    QVariantList searchActions(const QString &query, const QVariantMap &contextFilter) const;
    QVariantList getQuickActions(const QString &appId) const;

    QVariantMap invokeAction(const QString &actionId,
                             const QString &contextId,
                             const QVariantMap &parameters,
                             bool fromFrontend,
                             QString *error = nullptr);

signals:
    void providerRegistered(const QString &providerId, const QString &appId);
    void providerUnregistered(const QString &providerId);
    void contextChanged(const QString &contextId, const QVariantMap &context);
    void activeContextChanged(const QVariantMap &context);
    void actionsChanged(const QString &contextId, const QVariantList &actions);
    void actionStateChanged(const QString &contextId, const QString &actionId, const QVariantMap &action);
    void actionInvoked(const QString &actionId, const QString &contextId, const QVariantMap &result);
    void providerFailed(const QString &providerId, const QString &reason);
    void menuModelChanged(const QString &contextId, const QVariantList &menuModel);
    void searchIndexChanged();

private:
    struct ProviderInfo {
        QString providerId;
        QString appId;
        QVariantMap capabilities;
        QString ownerBus;
    };

    bool checkProviderOwnership(const QString &caller, const QString &providerId, QString *error) const;
    QVariantMap normalizeContext(const QString &providerId, const QVariantMap &context) const;
    QVariantMap normalizeAction(const QString &providerId, const QString &contextId, const QVariantMap &action) const;
    void ensureFallbackProvider();
    void recomputeActiveContext();
    int contextRank(const QVariantMap &ctx) const;
    QString resolveContextId(const QString &contextId) const;
    QVariantList sortedActions(const QVariantList &actions) const;
    bool isDestructiveAction(const QVariantMap &action) const;
    QVariantMap executeDesktopViewAction(const QVariantMap &action,
                                         const QVariantMap &context,
                                         const QVariantMap &parameters) const;
    bool openPathInFileManager(const QString &path, QString *error = nullptr) const;
    bool revealUriInFileManager(const QString &uri, QString *error = nullptr) const;
    QString uniqueChildPath(const QString &baseDir,
                            const QString &baseName,
                            const QString &suffix = QString()) const;

    QHash<QString, ProviderInfo> m_providers;
    QHash<QString, QVariantMap> m_contexts;
    QHash<QString, QHash<QString, QVariantMap>> m_actionsByContext;
    QString m_activeContextId;
};

} // namespace Slm::Actiond
