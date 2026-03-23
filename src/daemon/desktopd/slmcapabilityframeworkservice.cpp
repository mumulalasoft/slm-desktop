#include "slmcapabilityframeworkservice.h"

SlmCapabilityFrameworkService::SlmCapabilityFrameworkService(
    Slm::Actions::Framework::SlmActionFramework *framework,
    QObject *parent)
    : QObject(parent)
    , m_framework(framework)
{
}

QVariantList SlmCapabilityFrameworkService::ListActions(const QString &capability,
                                                        const QVariantMap &context)
{
    if (!m_framework) {
        return {};
    }
    return m_framework->listActions(capability, context);
}

QVariantMap SlmCapabilityFrameworkService::InvokeAction(const QString &actionId,
                                                        const QVariantMap &context)
{
    if (!m_framework) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("framework-unavailable")},
        };
    }
    return m_framework->invokeAction(actionId, context);
}

QVariantList SlmCapabilityFrameworkService::SearchActions(const QString &query,
                                                          const QVariantMap &context)
{
    if (!m_framework) {
        return {};
    }
    return m_framework->searchActions(query, context);
}

QVariantMap SlmCapabilityFrameworkService::ResolveDefaultAction(const QString &capability,
                                                                const QVariantMap &context)
{
    if (!m_framework) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("framework-unavailable")},
        };
    }
    return m_framework->resolveDefaultAction(capability, context);
}

