#pragma once

#include <QHash>
#include <QString>

namespace Slm::Desktopd {

struct SessionUnlockThrottleState {
    int failStreak = 0;
    int level = 0;
    qint64 lockoutUntilMs = 0;
    qint64 lastFailureMs = 0;
};

class SessionUnlockThrottle
{
public:
    static int lockoutSecondsForLevel(int level);
    static int decayWindowSeconds();
    static bool applyDecay(SessionUnlockThrottleState &state, qint64 nowMs);
    static void recordFailure(SessionUnlockThrottleState &state, qint64 nowMs);
    static void recordSuccess(SessionUnlockThrottleState &state);

    static bool load(const QString &path,
                     QHash<QString, SessionUnlockThrottleState> &states,
                     qint64 nowMs,
                     bool *changed = nullptr);
    static bool save(const QString &path,
                     const QHash<QString, SessionUnlockThrottleState> &states);
};

} // namespace Slm::Desktopd

