#include "slmactioninvoker.h"

namespace Slm::Actions::Framework {

ActionInvoker::ActionInvoker(ActionRegistry *registry)
    : m_registry(registry)
{
}

void ActionInvoker::setRegistry(ActionRegistry *registry)
{
    m_registry = registry;
}

QVariantMap ActionInvoker::invoke(const QString &actionId, const QVariantMap &context) const
{
    if (!m_registry) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("registry-unavailable")},
        };
    }
    return m_registry->invokeActionWithContext(actionId, context);
}

} // namespace Slm::Actions::Framework

