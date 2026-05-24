#include "actiondadaptors.h"

#include <QDBusMessage>
#include <QDebug>

namespace Slm::Actiond {

ActiondProviderAdaptor::ActiondProviderAdaptor(ActionDaemon *daemon)
    : QDBusAbstractAdaptor(daemon)
    , m_daemon(daemon)
{
    connect(m_daemon, &ActionDaemon::providerRegistered, this, &ActiondProviderAdaptor::ProviderRegistered);
    connect(m_daemon, &ActionDaemon::providerUnregistered, this, &ActiondProviderAdaptor::ProviderUnregistered);
    connect(m_daemon, &ActionDaemon::contextChanged, this, &ActiondProviderAdaptor::ContextChanged);
    connect(m_daemon, &ActionDaemon::actionsChanged, this, &ActiondProviderAdaptor::ActionsChanged);
    connect(m_daemon, &ActionDaemon::actionStateChanged, this, &ActiondProviderAdaptor::ActionStateChanged);
    connect(m_daemon, &ActionDaemon::actionInvoked, this, &ActiondProviderAdaptor::ActionInvoked);
    connect(m_daemon, &ActionDaemon::providerFailed, this, &ActiondProviderAdaptor::ProviderFailed);
}

bool ActiondProviderAdaptor::RegisterProvider(const QString &provider_id, const QString &app_id, const QVariantMap &capabilities)
{
    QString error;
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_daemon->registerProvider(caller, provider_id, app_id, capabilities, &error);
    if (!ok) {
        emit ProviderFailed(provider_id, error);
    }
    return ok;
}

bool ActiondProviderAdaptor::UnregisterProvider(const QString &provider_id)
{
    QString error;
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_daemon->unregisterProvider(caller, provider_id, &error);
    if (!ok) {
        emit ProviderFailed(provider_id, error);
    }
    return ok;
}

bool ActiondProviderAdaptor::RegisterContext(const QString &provider_id, const QVariantMap &context)
{
    QString error;
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_daemon->registerContext(caller, provider_id, context, &error);
    if (!ok) {
        emit ProviderFailed(provider_id, error);
    }
    return ok;
}

bool ActiondProviderAdaptor::UpdateContext(const QString &provider_id, const QVariantMap &context)
{
    return RegisterContext(provider_id, context);
}

bool ActiondProviderAdaptor::ActivateContext(const QString &context_id)
{
    QString error;
    return m_daemon->activateContext(context_id, &error);
}

bool ActiondProviderAdaptor::DeactivateContext(const QString &context_id)
{
    QString error;
    return m_daemon->deactivateContext(context_id, &error);
}

bool ActiondProviderAdaptor::SetActions(const QString &provider_id, const QString &context_id, const QVariantList &actions)
{
    QString error;
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_daemon->setActions(caller, provider_id, context_id, actions, &error);
    if (!ok) {
        emit ProviderFailed(provider_id, error);
    }
    return ok;
}

bool ActiondProviderAdaptor::UpdateAction(const QString &provider_id,
                                          const QString &context_id,
                                          const QString &action_id,
                                          const QVariantMap &patch)
{
    QString error;
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_daemon->updateAction(caller, provider_id, context_id, action_id, patch, &error);
    if (!ok) {
        emit ProviderFailed(provider_id, error);
    }
    return ok;
}

bool ActiondProviderAdaptor::RemoveAction(const QString &provider_id,
                                          const QString &context_id,
                                          const QString &action_id)
{
    QString error;
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_daemon->removeAction(caller, provider_id, context_id, action_id, &error);
    if (!ok) {
        emit ProviderFailed(provider_id, error);
    }
    return ok;
}

QVariantMap ActiondProviderAdaptor::InvokeAction(const QString &action_id,
                                                 const QString &context_id,
                                                 const QVariantMap &parameters)
{
    QString error;
    QVariantMap out = m_daemon->invokeAction(action_id, context_id, parameters, false, &error);
    if (!error.isEmpty()) {
        out.insert(QStringLiteral("ok"), false);
        out.insert(QStringLiteral("error"), error);
    }
    return out;
}

ActiondFrontendAdaptor::ActiondFrontendAdaptor(ActionDaemon *daemon)
    : QDBusAbstractAdaptor(daemon)
    , m_daemon(daemon)
{
    connect(m_daemon, &ActionDaemon::activeContextChanged, this, &ActiondFrontendAdaptor::ActiveContextChanged);
    connect(m_daemon, &ActionDaemon::menuModelChanged, this, &ActiondFrontendAdaptor::MenuModelChanged);
    connect(m_daemon, &ActionDaemon::actionsChanged, this, &ActiondFrontendAdaptor::ActionsChanged);
    connect(m_daemon, &ActionDaemon::searchIndexChanged, this, &ActiondFrontendAdaptor::SearchIndexChanged);
}

QVariantMap ActiondFrontendAdaptor::GetActiveContext() const
{
    return m_daemon->getActiveContext();
}

QVariantList ActiondFrontendAdaptor::GetProviders() const
{
    return m_daemon->getProviders();
}

QVariantList ActiondFrontendAdaptor::GetContexts() const
{
    return m_daemon->getContexts();
}

QVariantList ActiondFrontendAdaptor::GetActions(const QString &context_id) const
{
    return m_daemon->getActions(context_id);
}

QVariantList ActiondFrontendAdaptor::GetMenuModel(const QString &context_id) const
{
    return m_daemon->getMenuModel(context_id);
}

QVariantList ActiondFrontendAdaptor::SearchActions(const QString &query, const QVariantMap &context_filter) const
{
    return m_daemon->searchActions(query, context_filter);
}

QVariantList ActiondFrontendAdaptor::GetQuickActions(const QString &app_id) const
{
    return m_daemon->getQuickActions(app_id);
}

QVariantMap ActiondFrontendAdaptor::InvokeAction(const QString &action_id,
                                                 const QString &context_id,
                                                 const QVariantMap &parameters)
{
    QString error;
    QVariantMap out = m_daemon->invokeAction(action_id, context_id, parameters, true, &error);
    if (!error.isEmpty()) {
        out.insert(QStringLiteral("ok"), false);
        out.insert(QStringLiteral("error"), error);
    }
    return out;
}

} // namespace Slm::Actiond
