#pragma once

#include "AccessContext.h"
#include "CallerIdentity.h"

namespace Slm::Permissions {

class SensitiveContentPolicy {
public:
    bool shouldBlock(const CallerIdentity &caller, const AccessContext &context) const;
};

} // namespace Slm::Permissions

