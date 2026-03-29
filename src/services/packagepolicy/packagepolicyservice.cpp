#include "packagepolicyservice.h"

#include "aptsimulator.h"
#include "packagepolicyconfig.h"
#include "packagepolicyengine.h"
#include "packagepolicylogger.h"

#include <QJsonArray>
#include <QJsonDocument>

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

    const PolicyDecision decision = PackagePolicyEngine::evaluate(transaction, config.protectedPackages());

    QJsonObject decisionJson = decision.toJson();
    decisionJson.insert(QStringLiteral("protectedPackageCount"), static_cast<int>(config.protectedPackages().size()));

    QJsonObject transactionJson = transaction.toJson();
    if (!transaction.rawOutput.isEmpty()) {
        transactionJson.insert(QStringLiteral("raw"), transaction.rawOutput.left(8000));
    }

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
