#pragma once

#include <QString>

#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"

namespace Slm::Login {

enum class RecoveryRollbackSource {
    None,
    LastGoodSnapshot,
    Previous,
    Safe
};

RecoveryRollbackSource rollbackRecoveryConfig(ConfigManager &config,
                                              const SessionState &state,
                                              QString *error = nullptr);

QString recoveryRollbackSourceToString(RecoveryRollbackSource source);

} // namespace Slm::Login

