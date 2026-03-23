#include "daemonservicebootstrap.h"

namespace Slm::Bootstrap {

bool DaemonServiceBootstrap::shouldSpawnOnAttempt(int attempt)
{
    if (attempt <= 0) {
        return false;
    }
    return attempt == 1 || (attempt % 3) == 0;
}

bool DaemonServiceBootstrap::shouldStopRetry(bool serviceReady, int attempt)
{
    if (serviceReady) {
        return true;
    }
    return attempt >= kMaxAttempts;
}

} // namespace Slm::Bootstrap

