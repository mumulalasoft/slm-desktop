#pragma once

#include "packagetransaction.h"

#include <QString>
#include <QStringList>

namespace Slm::PackagePolicy {

class AptSimulator
{
public:
    static bool simulateApt(const QStringList &args,
                            PackageTransaction *transaction,
                            QString *error,
                            QString *errorCode = nullptr);
    static bool parseAptSimulationOutput(const QString &output,
                                         PackageTransaction *transaction,
                                         QString *error = nullptr);
    static bool parseDpkgIntent(const QStringList &args, PackageTransaction *transaction, QString *error = nullptr);
    static QString classifyAptFailure(const QString &output);

private:
    static QString normalizePackageName(const QString &name);
};

} // namespace Slm::PackagePolicy
