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
    void parseAptSimulationOutput_autoremove_extractsRemovals();
    void parseAptSimulationOutput_detectsReplacesProvider();
    void classifyAptFailure_dependencyConflictAndLock();
    void parseDpkgIntent_handlesDirectDebInstallAndRemove();
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

void PackagePolicyEngineTest::parseAptSimulationOutput_autoremove_extractsRemovals()
{
    const QString sample = QStringLiteral(
        "Reading package lists... Done\n"
        "Building dependency tree... Done\n"
        "The following packages will be REMOVED:\n"
        "  gvfs-backends libexample1\n"
        "Remv gvfs-backends [1.54.0-1]\n"
        "Remv libexample1 [2.0-1]\n");

    PackageTransaction tx;
    QString error;
    QVERIFY(AptSimulator::parseAptSimulationOutput(sample, &tx, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(tx.remove.contains(QStringLiteral("gvfs-backends")));
    QVERIFY(tx.remove.contains(QStringLiteral("libexample1")));
}

void PackagePolicyEngineTest::classifyAptFailure_dependencyConflictAndLock()
{
    const QString dependencyConflict = QStringLiteral(
        "E: Unable to correct problems, you have held broken packages.\n"
        "The following packages have unmet dependencies:");
    QCOMPARE(AptSimulator::classifyAptFailure(dependencyConflict),
             QStringLiteral("dependency-conflict"));

    const QString aptLock = QStringLiteral(
        "E: Could not get lock /var/lib/dpkg/lock-frontend. It is held by process 1234");
    QCOMPARE(AptSimulator::classifyAptFailure(aptLock),
             QStringLiteral("apt-lock-busy"));
}

void PackagePolicyEngineTest::parseAptSimulationOutput_detectsReplacesProvider()
{
    const QString sample = QStringLiteral(
        "Inst vendor-gl-driver (2.0 Vendor:stable) Replacing mesa\n"
        "Upgr another-provider (3.1 Vendor:stable) Replaces libgl1\n");

    PackageTransaction tx;
    QString error;
    QVERIFY(AptSimulator::parseAptSimulationOutput(sample, &tx, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(tx.replace.contains(QStringLiteral("mesa")));
    QVERIFY(tx.replace.contains(QStringLiteral("libgl1")));
}

void PackagePolicyEngineTest::parseDpkgIntent_handlesDirectDebInstallAndRemove()
{
    PackageTransaction installTx;
    QString error;
    QVERIFY(AptSimulator::parseDpkgIntent(
        {QStringLiteral("-i"), QStringLiteral("/tmp/samba_2.0.1-1_amd64.deb")},
        &installTx,
        &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(installTx.install, QStringList({QStringLiteral("samba")}));

    PackageTransaction removeTx;
    QVERIFY(AptSimulator::parseDpkgIntent(
        {QStringLiteral("--remove"), QStringLiteral("libc6:amd64")},
        &removeTx,
        &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(removeTx.remove, QStringList({QStringLiteral("libc6")}));
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
