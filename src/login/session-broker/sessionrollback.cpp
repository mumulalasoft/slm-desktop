#include "sessionrollback.h"

namespace Slm::Login {

RecoveryRollbackSource rollbackRecoveryConfig(ConfigManager &config,
                                              const SessionState &state,
                                              QString *error)
{
    QString localError;

    const QString snapshotId = state.lastGoodSnapshot.trimmed();
    if (!snapshotId.isEmpty()) {
        if (config.restoreSnapshot(snapshotId, &localError)) {
            if (error) {
                error->clear();
            }
            return RecoveryRollbackSource::LastGoodSnapshot;
        }
    }

    if (config.hasPreviousConfig()) {
        if (config.rollbackToPrevious(&localError)) {
            if (error) {
                error->clear();
            }
            return RecoveryRollbackSource::Previous;
        }
    }

    if (config.rollbackToSafe(&localError)) {
        if (error) {
            error->clear();
        }
        return RecoveryRollbackSource::Safe;
    }

    if (error) {
        *error = localError;
    }
    return RecoveryRollbackSource::None;
}

QString recoveryRollbackSourceToString(RecoveryRollbackSource source)
{
    switch (source) {
    case RecoveryRollbackSource::None:
        return QStringLiteral("none");
    case RecoveryRollbackSource::LastGoodSnapshot:
        return QStringLiteral("last_good_snapshot");
    case RecoveryRollbackSource::Previous:
        return QStringLiteral("previous");
    case RecoveryRollbackSource::Safe:
        return QStringLiteral("safe");
    }
    return QStringLiteral("none");
}

} // namespace Slm::Login

