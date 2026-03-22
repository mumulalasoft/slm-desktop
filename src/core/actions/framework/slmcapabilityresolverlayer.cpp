#include "slmcapabilityresolverlayer.h"

namespace Slm::Actions::Framework {

CapabilityResolverLayer::CapabilityResolverLayer(ActionRegistry *registry)
    : m_registry(registry)
{
}

void CapabilityResolverLayer::setRegistry(ActionRegistry *registry)
{
    m_registry = registry;
}

ActionRegistry *CapabilityResolverLayer::registry() const
{
    return m_registry;
}

QVariantList CapabilityResolverLayer::resolve(const QString &capability,
                                              const QVariantMap &context) const
{
    if (!m_registry) {
        return {};
    }
    return m_registry->listActionsWithContext(capability, context);
}

} // namespace Slm::Actions::Framework

