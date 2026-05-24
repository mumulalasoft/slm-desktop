#pragma once

#include "actiondaemon.h"

#include <QDBusContext>
#include <QDBusAbstractAdaptor>

namespace Slm::Actiond {

class ActiondProviderAdaptor : public QDBusAbstractAdaptor, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Actiond.Provider")

public:
    explicit ActiondProviderAdaptor(ActionDaemon *daemon);

public slots:
    bool RegisterProvider(const QString &provider_id, const QString &app_id, const QVariantMap &capabilities);
    bool UnregisterProvider(const QString &provider_id);

    bool RegisterContext(const QString &provider_id, const QVariantMap &context);
    bool UpdateContext(const QString &provider_id, const QVariantMap &context);
    bool ActivateContext(const QString &context_id);
    bool DeactivateContext(const QString &context_id);

    bool SetActions(const QString &provider_id, const QString &context_id, const QVariantList &actions);
    bool UpdateAction(const QString &provider_id, const QString &context_id, const QString &action_id, const QVariantMap &patch);
    bool RemoveAction(const QString &provider_id, const QString &context_id, const QString &action_id);

    QVariantMap InvokeAction(const QString &action_id, const QString &context_id, const QVariantMap &parameters);

signals:
    void ProviderRegistered(const QString &provider_id, const QString &app_id);
    void ProviderUnregistered(const QString &provider_id);
    void ContextChanged(const QString &context_id, const QVariantMap &context);
    void ActionsChanged(const QString &context_id, const QVariantList &actions);
    void ActionStateChanged(const QString &context_id, const QString &action_id, const QVariantMap &action);
    void ActionInvoked(const QString &action_id, const QString &context_id, const QVariantMap &result);
    void ProviderFailed(const QString &provider_id, const QString &reason);

private:
    ActionDaemon *m_daemon = nullptr;
};

class ActiondFrontendAdaptor : public QDBusAbstractAdaptor, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Actiond.Frontend")

public:
    explicit ActiondFrontendAdaptor(ActionDaemon *daemon);

public slots:
    QVariantMap GetActiveContext() const;
    QVariantList GetProviders() const;
    QVariantList GetContexts() const;
    QVariantList GetActions(const QString &context_id) const;
    QVariantList GetMenuModel(const QString &context_id) const;
    QVariantList SearchActions(const QString &query, const QVariantMap &context_filter) const;
    QVariantList GetQuickActions(const QString &app_id) const;
    QVariantMap InvokeAction(const QString &action_id, const QString &context_id, const QVariantMap &parameters);

signals:
    void ActiveContextChanged(const QVariantMap &context);
    void MenuModelChanged(const QString &context_id, const QVariantList &menu_model);
    void ActionsChanged(const QString &context_id, const QVariantList &actions);
    void SearchIndexChanged();

private:
    ActionDaemon *m_daemon = nullptr;
};

} // namespace Slm::Actiond
