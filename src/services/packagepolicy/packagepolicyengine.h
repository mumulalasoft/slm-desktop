#pragma once

#include "packagetransaction.h"
#include "packagepolicyconfig.h"
#include "packagesourceresolver.h"

#include <QJsonObject>
#include <QHash>
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
                                   const QSet<QString> &protectedPackages,
                                   const QHash<QString, PackageSourceInfo> &packageSources = {},
                                   const QHash<QString, SourcePolicy> &sourcePolicies = {});
};

} // namespace Slm::PackagePolicy
