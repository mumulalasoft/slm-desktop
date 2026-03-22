#pragma once

#include "../slmactionregistry.h"

#include <QVariantList>
#include <QVariantMap>

namespace Slm::Actions::Framework {

class CapabilityResolverLayer
{
public:
    explicit CapabilityResolverLayer(ActionRegistry *registry = nullptr);

    void setRegistry(ActionRegistry *registry);
    ActionRegistry *registry() const;

    QVariantList resolve(const QString &capability, const QVariantMap &context) const;

private:
    ActionRegistry *m_registry = nullptr;
};

} // namespace Slm::Actions::Framework

