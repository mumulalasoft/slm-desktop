#include "packagepolicyengine.h"

#include <QJsonArray>
#include <algorithm>

namespace Slm::PackagePolicy {

namespace {
QStringList sortedIntersection(const QStringList &rows, const QSet<QString> &protectedPackages)
{
    QStringList out;
    for (const QString &row : rows) {
        if (protectedPackages.contains(row)) {
            out.push_back(row);
        }
    }
    out.removeDuplicates();
    std::sort(out.begin(), out.end());
    return out;
}

QJsonArray toJsonArray(const QStringList &rows)
{
    QJsonArray out;
    for (const QString &row : rows) {
        out.push_back(row);
    }
    return out;
}
} // namespace

QJsonObject PolicyDecision::toJson() const
{
    QJsonObject out;
    out.insert(QStringLiteral("allowed"), allowed);
    out.insert(QStringLiteral("warnings"), toJsonArray(warnings));
    out.insert(QStringLiteral("blockReasons"), toJsonArray(blockReasons));
    return out;
}

PolicyDecision PackagePolicyEngine::evaluate(const PackageTransaction &transaction,
                                             const QSet<QString> &protectedPackages,
                                             const QHash<QString, PackageSourceInfo> &packageSources,
                                             const QHash<QString, SourcePolicy> &sourcePolicies)
{
    PolicyDecision decision;

    const QStringList removedProtected = sortedIntersection(transaction.remove, protectedPackages);
    if (!removedProtected.isEmpty()) {
        decision.allowed = false;
        decision.blockReasons.push_back(
            QStringLiteral("blocked: removing protected packages: %1")
                .arg(removedProtected.join(QStringLiteral(", "))));
    }

    const QStringList replacedProtected = sortedIntersection(transaction.replace, protectedPackages);
    if (!replacedProtected.isEmpty()) {
        decision.allowed = false;
        decision.blockReasons.push_back(
            QStringLiteral("blocked: replacing protected packages: %1")
                .arg(replacedProtected.join(QStringLiteral(", "))));
    }

    const QStringList downgradedProtected = sortedIntersection(transaction.downgrade, protectedPackages);
    if (!downgradedProtected.isEmpty()) {
        decision.allowed = false;
        decision.blockReasons.push_back(
            QStringLiteral("blocked: downgrading protected packages: %1")
                .arg(downgradedProtected.join(QStringLiteral(", "))));
    }

    QStringList installOrUpgrade;
    installOrUpgrade << transaction.install << transaction.upgrade;
    installOrUpgrade.removeDuplicates();
    std::sort(installOrUpgrade.begin(), installOrUpgrade.end());

    QStringList trustedExternalPackages;
    QStringList highRiskExternalPackages;
    QStringList externalReplacingProtected;

    for (const QString &pkg : installOrUpgrade) {
        const PackageSourceInfo sourceInfo = packageSources.value(pkg);
        if (sourceInfo.sourceId.isEmpty()) {
            continue;
        }
        const SourcePolicy sourcePolicy = sourcePolicies.value(sourceInfo.sourceId);
        const QString kind = sourcePolicy.kind.isEmpty() ? sourceInfo.sourceKind : sourcePolicy.kind;
        const QString risk = sourcePolicy.risk.isEmpty() ? sourceInfo.sourceRisk : sourcePolicy.risk;
        const bool canReplaceCore = sourcePolicies.contains(sourceInfo.sourceId)
                                        ? sourcePolicy.canReplaceCore
                                        : (sourceInfo.sourceId == QLatin1String("official"));

        if (kind != QLatin1String("official")) {
            if (kind == QLatin1String("trusted-external")) {
                trustedExternalPackages.push_back(pkg);
            } else {
                highRiskExternalPackages.push_back(pkg);
            }
        }

        if (!replacedProtected.isEmpty() && kind != QLatin1String("official") && !canReplaceCore) {
            externalReplacingProtected.push_back(
                QStringLiteral("%1(%2,%3)")
                    .arg(pkg,
                         sourceInfo.sourceId,
                         risk.isEmpty() ? QStringLiteral("unknown") : risk));
        }
    }

    trustedExternalPackages.removeDuplicates();
    std::sort(trustedExternalPackages.begin(), trustedExternalPackages.end());
    highRiskExternalPackages.removeDuplicates();
    std::sort(highRiskExternalPackages.begin(), highRiskExternalPackages.end());
    externalReplacingProtected.removeDuplicates();
    std::sort(externalReplacingProtected.begin(), externalReplacingProtected.end());

    if (!externalReplacingProtected.isEmpty()) {
        decision.allowed = false;
        decision.blockReasons.push_back(
            QStringLiteral("blocked: external source cannot replace protected packages: %1")
                .arg(externalReplacingProtected.join(QStringLiteral(", "))));
    }

    if (!trustedExternalPackages.isEmpty()) {
        decision.warnings.push_back(
            QStringLiteral("warning: trusted external source packages: %1")
                .arg(trustedExternalPackages.join(QStringLiteral(", "))));
    }
    if (!highRiskExternalPackages.isEmpty()) {
        decision.warnings.push_back(
            QStringLiteral("warning: high-risk external/local packages: %1")
                .arg(highRiskExternalPackages.join(QStringLiteral(", "))));
    }

    if (decision.allowed) {
        decision.warnings.push_back(QStringLiteral("allowed: transaction does not touch protected packages"));
    }

    return decision;
}

} // namespace Slm::PackagePolicy
