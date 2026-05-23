#pragma once

#include <QString>

namespace Slm::Installer {

struct SlmError
{
    QString code;
    QString userMessage;
    QString suggestedAction;
    bool retryable = false;
};

// Translate a raw subprocess outcome into a human-readable SlmError.
// Mapping is performed via a static table (not regex) for determinism.
// See docs/SLM_INSTALLER_BACKEND.md §9.
class SlmErrorTranslator
{
public:
    static SlmError translate(const QString &phase, int exitCode, const QString &rawOutput);
};

} // namespace Slm::Installer
