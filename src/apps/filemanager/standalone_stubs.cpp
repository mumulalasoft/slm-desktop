// Standalone stubs for symbols referenced by slm-filemanager-core but whose
// real implementations live in the shell (slm-desktop) launcher stack.
//
// The standalone file manager never constructs an AppExecutionGate, so these
// bodies should not execute at runtime — ActionRegistry::invokeActionWithContext
// short-circuits with "execution-gate-unavailable" when m_executionGate is null.
// They exist purely so the linker is happy when slmactionregistry.cpp emits a
// direct call to AppExecutionGate::launchCommand.

#include "src/core/execution/appexecutiongate.h"

bool AppExecutionGate::launchCommand(const QString &, const QString &, const QString &)
{
    return false;
}
