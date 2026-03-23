#pragma once

namespace Slm::Bootstrap {

class DaemonServiceBootstrap
{
public:
    static constexpr int kRetryIntervalMs = 1500;
    static constexpr int kMaxAttempts = 12;

    static bool shouldSpawnOnAttempt(int attempt);
    static bool shouldStopRetry(bool serviceReady, int attempt);
};

} // namespace Slm::Bootstrap

