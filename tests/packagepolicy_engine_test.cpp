#include <QtTest>

#include "src/services/packagepolicy/aptsimulator.h"
#include "src/services/packagepolicy/packagepolicyengine.h"

using Slm::PackagePolicy::AptSimulator;
using Slm::PackagePolicy::PackagePolicyEngine;
using Slm::PackagePolicy::PackageTransaction;

class PackagePolicyEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void parseAptSimulationOutput_extractsTransactions();
    void evaluate_blocksProtectedRemoval();
    void evaluate_allowsSafeTransaction();
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

QTEST_MAIN(PackagePolicyEngineTest)
#include "packagepolicy_engine_test.moc"
