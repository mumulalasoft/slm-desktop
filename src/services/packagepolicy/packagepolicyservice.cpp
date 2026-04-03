#include "packagepolicyservice.h"

#include "aptsimulator.h"
#include "packagepolicyconfig.h"
#include "packagepolicyengine.h"
#include "packagepolicylogger.h"
#include "packagesourceresolver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace Slm::PackagePolicy {

namespace {
QVariantMap buildError(const QString &message)
{
    QVariantMap out;
    out.insert(QStringLiteral("allowed"), false);
    out.insert(QStringLiteral("error"), message);
    return out;
}

QString sourceRisk(const PackageSourceInfo &info)
{
    return info.sourceRisk.trimmed().toLower();
}

QString sourceId(const PackageSourceInfo &info)
{
    return info.sourceId.trimmed().toLower();
}

QString summarizeTrustLevel(const QHash<QString, PackageSourceInfo> &packageSources)
{
    if (packageSources.isEmpty()) {
        return QStringLiteral("official");
    }
    QSet<QString> ids;
    for (auto it = packageSources.constBegin(); it != packageSources.constEnd(); ++it) {
        const QString id = sourceId(it.value());
        if (!id.isEmpty()) {
            ids.insert(id);
        }
    }
    if (ids.isEmpty()) {
        return QStringLiteral("unknown");
    }
    if (ids.size() == 1) {
        return *ids.constBegin();
    }
    return QStringLiteral("mixed");
}

QString summarizeRiskLevel(const PolicyDecision &decision,
                           const QHash<QString, PackageSourceInfo> &packageSources,
                           const QJsonArray &touchedProtectedAll)
{
    if (!decision.allowed || !touchedProtectedAll.isEmpty()) {
        return QStringLiteral("high");
    }
    bool hasMedium = false;
    for (auto it = packageSources.constBegin(); it != packageSources.constEnd(); ++it) {
        const QString risk = sourceRisk(it.value());
        if (risk == QLatin1String("high")) {
            return QStringLiteral("high");
        }
        if (risk == QLatin1String("medium")) {
            hasMedium = true;
        }
    }
    return hasMedium ? QStringLiteral("medium") : QStringLiteral("low");
}

QString summarizeImpactMessage(const PolicyDecision &decision,
                               const QJsonArray &touchedProtectedAll,
                               const QString &riskLevel)
{
    if (!decision.allowed && !touchedProtectedAll.isEmpty()) {
        return QStringLiteral("Instalasi diblokir karena memengaruhi komponen inti sistem.");
    }
    if (!decision.allowed) {
        return QStringLiteral("Instalasi diblokir oleh kebijakan keamanan paket.");
    }
    if (riskLevel == QLatin1String("high")) {
        return QStringLiteral("Transaksi berisiko tinggi. Tinjau sumber paket sebelum lanjut.");
    }
    if (riskLevel == QLatin1String("medium")) {
        return QStringLiteral("Paket ini berasal dari sumber eksternal tepercaya.");
    }
    return QStringLiteral("Tidak ada dampak ke komponen inti terdeteksi.");
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
        QString simulationErrorCode;
        if (!AptSimulator::simulateApt(args, &transaction, &simulationError, &simulationErrorCode)) {
            QVariantMap out = buildError(simulationErrorCode.isEmpty()
                                             ? QStringLiteral("apt-simulation-failed")
                                             : simulationErrorCode);
            out.insert(QStringLiteral("message"), simulationError);
            return out;
        }
    } else if (normalizedTool == QLatin1String("dpkg")) {
        if (!AptSimulator::parseDpkgIntent(args, &transaction, &simulationError)) {
            QVariantMap out = buildError(QStringLiteral("dpkg-intent-parse-failed"));
            out.insert(QStringLiteral("message"), simulationError);
            return out;
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

    QJsonArray touchedProtectedRemove;
    QJsonArray touchedProtectedReplace;
    QJsonArray touchedProtectedDowngrade;
    QSet<QString> touchedProtectedAllSet;
    for (const QString &pkg : transaction.remove) {
        if (config.protectedPackages().contains(pkg)) {
            touchedProtectedRemove.push_back(pkg);
            touchedProtectedAllSet.insert(pkg);
        }
    }
    for (const QString &pkg : transaction.replace) {
        if (config.protectedPackages().contains(pkg)) {
            touchedProtectedReplace.push_back(pkg);
            touchedProtectedAllSet.insert(pkg);
        }
    }
    for (const QString &pkg : transaction.downgrade) {
        if (config.protectedPackages().contains(pkg)) {
            touchedProtectedDowngrade.push_back(pkg);
            touchedProtectedAllSet.insert(pkg);
        }
    }
    QJsonArray touchedProtectedAll;
    QStringList touchedProtectedRows = QStringList(touchedProtectedAllSet.begin(), touchedProtectedAllSet.end());
    std::sort(touchedProtectedRows.begin(), touchedProtectedRows.end());
    for (const QString &pkg : touchedProtectedRows) {
        touchedProtectedAll.push_back(pkg);
    }

    decisionJson.insert(QStringLiteral("touchedProtectedRemove"), touchedProtectedRemove);
    decisionJson.insert(QStringLiteral("touchedProtectedReplace"), touchedProtectedReplace);
    decisionJson.insert(QStringLiteral("touchedProtectedDowngrade"), touchedProtectedDowngrade);
    decisionJson.insert(QStringLiteral("touchedProtectedAny"), touchedProtectedAll);

    const QString trustLevel = summarizeTrustLevel(packageSources);
    const QString riskLevel = summarizeRiskLevel(decision, packageSources, touchedProtectedAll);
    const QString impactMessage = summarizeImpactMessage(decision, touchedProtectedAll, riskLevel);
    decisionJson.insert(QStringLiteral("trustLevel"), trustLevel);
    decisionJson.insert(QStringLiteral("riskLevel"), riskLevel);
    decisionJson.insert(QStringLiteral("impactMessage"), impactMessage);

    PackagePolicyLogger::writeDecision(normalizedTool, args, decisionJson, transactionJson);

    QVariantMap out = decisionJson.toVariantMap();
    out.insert(QStringLiteral("transaction"), transactionJson.toVariantMap());
    return out;
}

} // namespace Slm::PackagePolicy
