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
                                             const QSet<QString> &protectedPackages)
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

    if (decision.allowed) {
        decision.warnings.push_back(QStringLiteral("allowed: transaction does not touch protected packages"));
    }

    return decision;
}

} // namespace Slm::PackagePolicy
