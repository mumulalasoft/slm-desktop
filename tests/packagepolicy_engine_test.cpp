#include <QtTest>

#include "src/services/packagepolicy/aptsimulator.h"
#include "src/services/packagepolicy/packagepolicyengine.h"
#include "src/services/packagepolicy/packagepolicyconfig.h"
#include "src/services/packagepolicy/packagesourceresolver.h"

using Slm::PackagePolicy::AptSimulator;
using Slm::PackagePolicy::PackagePolicyEngine;
using Slm::PackagePolicy::PackageSourceInfo;
using Slm::PackagePolicy::PackageTransaction;
using Slm::PackagePolicy::SourcePolicy;

class PackagePolicyEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void parseAptSimulationOutput_extractsTransactions();
    void evaluate_blocksProtectedRemoval();
    void evaluate_allowsSafeTransaction();
    void evaluate_blocksExternalReplaceProtected();
    void evaluate_warnsTrustedExternalInstall();
};

void PackagePolicyEngineTest::parseAptSimulationOutput_extractsTransactions()
{
    const QString sample = QStringLiteral(
        "Inst vim [2:9.0] (2:9.1 Ubuntu:24.04)\n"
        "Remv libc6 [2.39-0ubuntu8]\n"
        "Upgr nano [6.2] (6.3 Ubuntu:24.04)\n");

    PackageTransaction tx;
    QString error;
    QVERIFY(AptSimulator::parseAptSimulationOutput(sample, &tx, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QVERIFY(tx.install.contains(QStringLiteral("vim")));
    QVERIFY(tx.remove.contains(QStringLiteral("libc6")));
    QVERIFY(tx.upgrade.contains(QStringLiteral("nano")));
}

void PackagePolicyEngineTest::evaluate_blocksProtectedRemoval()
{
    PackageTransaction tx;
    tx.remove = {QStringLiteral("libc6"), QStringLiteral("vim")};

    const QSet<QString> protectedPackages = {
        QStringLiteral("libc6"),
        QStringLiteral("systemd")
    };

    const auto decision = PackagePolicyEngine::evaluate(tx, protectedPackages);
    QCOMPARE(decision.allowed, false);
    QVERIFY(!decision.blockReasons.isEmpty());
}

void PackagePolicyEngineTest::evaluate_allowsSafeTransaction()
{
    PackageTransaction tx;
    tx.install = {QStringLiteral("vim")};
    tx.upgrade = {QStringLiteral("nano")};

    const QSet<QString> protectedPackages = {
        QStringLiteral("libc6"),
        QStringLiteral("systemd")
    };

    const auto decision = PackagePolicyEngine::evaluate(tx, protectedPackages);
    QCOMPARE(decision.allowed, true);
    QVERIFY(decision.blockReasons.isEmpty());
}

void PackagePolicyEngineTest::evaluate_blocksExternalReplaceProtected()
{
    PackageTransaction tx;
    tx.install = {QStringLiteral("vendor-gl-driver")};
    tx.replace = {QStringLiteral("mesa")};

    const QSet<QString> protectedPackages = {
        QStringLiteral("mesa"),
        QStringLiteral("libc6")
    };

    QHash<QString, PackageSourceInfo> sources;
    PackageSourceInfo vendorInfo;
    vendorInfo.sourceId = QStringLiteral("vendor");
    vendorInfo.sourceKind = QStringLiteral("trusted-external");
    vendorInfo.sourceRisk = QStringLiteral("medium");
    sources.insert(QStringLiteral("vendor-gl-driver"), vendorInfo);

    QHash<QString, SourcePolicy> sourcePolicies;
    SourcePolicy official;
    official.id = QStringLiteral("official");
    official.kind = QStringLiteral("official");
    official.risk = QStringLiteral("low");
    official.canReplaceCore = true;
    sourcePolicies.insert(official.id, official);

    SourcePolicy vendor;
    vendor.id = QStringLiteral("vendor");
    vendor.kind = QStringLiteral("trusted-external");
    vendor.risk = QStringLiteral("medium");
    vendor.canReplaceCore = false;
    sourcePolicies.insert(vendor.id, vendor);

    const auto decision = PackagePolicyEngine::evaluate(tx, protectedPackages, sources, sourcePolicies);
    QCOMPARE(decision.allowed, false);
    QVERIFY(!decision.blockReasons.isEmpty());
    QVERIFY(decision.blockReasons.join(QStringLiteral(" ")).contains(QStringLiteral("external"), Qt::CaseInsensitive));
}

void PackagePolicyEngineTest::evaluate_warnsTrustedExternalInstall()
{
    PackageTransaction tx;
    tx.install = {QStringLiteral("vendor-note-app")};

    const QSet<QString> protectedPackages = {
        QStringLiteral("libc6"),
        QStringLiteral("systemd")
    };

    QHash<QString, PackageSourceInfo> sources;
    PackageSourceInfo vendorInfo;
    vendorInfo.sourceId = QStringLiteral("vendor");
    vendorInfo.sourceKind = QStringLiteral("trusted-external");
    vendorInfo.sourceRisk = QStringLiteral("medium");
    sources.insert(QStringLiteral("vendor-note-app"), vendorInfo);

    QHash<QString, SourcePolicy> sourcePolicies;
    SourcePolicy vendor;
    vendor.id = QStringLiteral("vendor");
    vendor.kind = QStringLiteral("trusted-external");
    vendor.risk = QStringLiteral("medium");
    vendor.canReplaceCore = false;
    sourcePolicies.insert(vendor.id, vendor);

    const auto decision = PackagePolicyEngine::evaluate(tx, protectedPackages, sources, sourcePolicies);
    QCOMPARE(decision.allowed, true);
    QVERIFY(decision.warnings.join(QStringLiteral(" ")).contains(QStringLiteral("trusted external"),
                                                                 Qt::CaseInsensitive));
}

QTEST_MAIN(PackagePolicyEngineTest)
#include "packagepolicy_engine_test.moc"
