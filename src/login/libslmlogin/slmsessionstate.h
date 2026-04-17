#pragma once
#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include "slmlogindefs.h"

namespace Slm::Login {

struct SessionState {
    int         crashCount       = 0;
    StartupMode lastMode         = StartupMode::Normal;
    QString     lastGoodSnapshot;        // snapshot id, empty if none
    bool        safeModeForced   = false;
    bool        configPending    = false; // true until watchdog confirms session healthy
    QString     recoveryReason;
    QString     lastBootStatus;          // "started" | "healthy" | "crashed" | "ended"
    QDateTime   lastUpdated;

    static SessionState defaults();
    QVariantMap toMap() const;
    static SessionState fromMap(const QVariantMap &map);
};

class SessionStateIO
{
public:
    // ~/.config/slm-desktop/state.json
    static QString statePath();

    // Loads from disk. Returns defaults silently if file missing.
    // Returns false (and populates outError) only on parse failure.
    static bool load(SessionState &out, QString &outError);

    // Atomic write: tmp file + rename.
    static bool save(const SessionState &state, QString &outError);
};

} // namespace Slm::Login
