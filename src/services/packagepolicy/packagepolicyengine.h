#pragma once

#include "packagetransaction.h"

#include <QJsonObject>
#include <QSet>

namespace Slm::PackagePolicy {

struct PolicyDecision {
    bool allowed = true;
    QStringList warnings;
    QStringList blockReasons;

    QJsonObject toJson() const;
};

class PackagePolicyEngine
{
public:
    static PolicyDecision evaluate(const PackageTransaction &transaction,
                                   const QSet<QString> &protectedPackages);
};

} // namespace Slm::PackagePolicy
