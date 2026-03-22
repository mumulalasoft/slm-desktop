#include "SensitiveContentPolicy.h"

namespace Slm::Permissions {

bool SensitiveContentPolicy::shouldBlock(const CallerIdentity &caller,
                                         const AccessContext &context) const
{
    if (context.sensitivityLevel != SensitivityLevel::Critical) {
        return false;
    }
    // Critical data is blocked for non-core callers by default.
    return caller.trustLevel != TrustLevel::CoreDesktopComponent;
}

} // namespace Slm::Permissions

