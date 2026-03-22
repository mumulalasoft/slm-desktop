#pragma once

#include "../slmactionregistry.h"

namespace Slm::Actions::Framework {

class ActionInvoker
{
public:
    explicit ActionInvoker(ActionRegistry *registry = nullptr);

    void setRegistry(ActionRegistry *registry);
    QVariantMap invoke(const QString &actionId, const QVariantMap &context) const;

private:
    ActionRegistry *m_registry = nullptr;
};

} // namespace Slm::Actions::Framework

