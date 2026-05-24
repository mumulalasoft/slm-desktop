#include "SlmErrorTranslator.h"

namespace Slm::Installer {

SlmError SlmErrorTranslator::translate(const QString &phase, int exitCode, const QString &rawOutput)
{
    Q_UNUSED(phase)
    Q_UNUSED(exitCode)
    Q_UNUSED(rawOutput)

    return SlmError{
        QStringLiteral("UNK_001"),
        QStringLiteral("An unexpected problem occurred."),
        QStringLiteral("Try again, or save a report for support."),
        true,
    };
}

} // namespace Slm::Installer
