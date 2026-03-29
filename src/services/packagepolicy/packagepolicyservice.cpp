#include "packagepolicyservice.h"

#include "aptsimulator.h"
#include "packagepolicyconfig.h"
#include "packagepolicyengine.h"
#include "packagepolicylogger.h"
#include "packagesourceresolver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Slm::PackagePolicy {

namespace {
QVariantMap buildError(const QString &message)
{
    QVariantMap out;
    out.insert(QStringLiteral("allowed"), false);
    out.insert(QStringLiteral("error"), message);
    return out;
}
} // namespace

PackagePolicyService::PackagePolicyService(QObject *parent)
    : QObject(parent)
{
}

QVariantMap PackagePolicyService::Evaluate(const QString &tool, const QStringList &args)
{
    return evaluateInternal(tool, args);
}

QVariantMap PackagePolicyService::evaluateInternal(const QString &tool, const QStringList &args)
{
    PackagePolicyConfig config;
    QString configError;
    if (!config.load(&configError)) {
        return buildError(configError);
    }

    PackageTransaction transaction;
    QString simulationError;

    const QString normalizedTool = tool.trimmed().toLower();
    if (normalizedTool.isEmpty()) {
        return buildError(QStringLiteral("tool is required"));
    }
    if (normalizedTool == QLatin1String("apt") || normalizedTool == QLatin1String("apt-get")) {
        if (!AptSimulator::simulateApt(args, &transaction, &simulationError)) {
            return buildError(simulationError);
        }
    } else if (normalizedTool == QLatin1String("dpkg")) {
        if (!AptSimulator::parseDpkgIntent(args, &transaction, &simulationError)) {
            return buildError(simulationError);
        }
    } else {
        return buildError(QStringLiteral("unsupported tool: %1").arg(tool));
    }

    QStringList sourceTargetPackages;
    sourceTargetPackages << transaction.install << transaction.upgrade;
    sourceTargetPackages.removeDuplicates();
    const QHash<QString, PackageSourceInfo> packageSources =
        PackageSourceResolver::detectForPackages(sourceTargetPackages, normalizedTool);

    const PolicyDecision decision = PackagePolicyEngine::evaluate(transaction,
                                                                  config.protectedPackages(),
                                                                  packageSources,
                                                                  config.sourcePolicies());

    QJsonObject decisionJson = decision.toJson();
    decisionJson.insert(QStringLiteral("protectedPackageCount"), static_cast<int>(config.protectedPackages().size()));

    QJsonObject transactionJson = transaction.toJson();
    if (!transaction.rawOutput.isEmpty()) {
        transactionJson.insert(QStringLiteral("raw"), transaction.rawOutput.left(8000));
    }

    QJsonObject packageSourcesJson;
    for (auto it = packageSources.constBegin(); it != packageSources.constEnd(); ++it) {
        QJsonObject row;
        row.insert(QStringLiteral("sourceId"), it.value().sourceId);
        row.insert(QStringLiteral("sourceKind"), it.value().sourceKind);
        row.insert(QStringLiteral("sourceRisk"), it.value().sourceRisk);
        row.insert(QStringLiteral("detail"), it.value().sourceDetail);
        packageSourcesJson.insert(it.key(), row);
    }
    decisionJson.insert(QStringLiteral("packageSources"), packageSourcesJson);

    QJsonArray touchedProtected;
    for (const QString &pkg : transaction.remove) {
        if (config.protectedPackages().contains(pkg)) {
            touchedProtected.push_back(pkg);
        }
    }
    decisionJson.insert(QStringLiteral("touchedProtectedRemove"), touchedProtected);

    PackagePolicyLogger::writeDecision(normalizedTool, args, decisionJson, transactionJson);

    QVariantMap out = decisionJson.toVariantMap();
    out.insert(QStringLiteral("transaction"), transactionJson.toVariantMap());
    return out;
}

} // namespace Slm::PackagePolicy
