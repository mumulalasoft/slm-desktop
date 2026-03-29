#pragma once

#include "packagetransaction.h"

#include <QString>
#include <QStringList>

namespace Slm::PackagePolicy {

class AptSimulator
{
public:
    static bool simulateApt(const QStringList &args, PackageTransaction *transaction, QString *error);
    static bool parseAptSimulationOutput(const QString &output,
                                         PackageTransaction *transaction,
                                         QString *error = nullptr);
    static bool parseDpkgIntent(const QStringList &args, PackageTransaction *transaction, QString *error = nullptr);

private:
    static QString normalizePackageName(const QString &name);
};

} // namespace Slm::PackagePolicy
