#include <QtTest/QtTest>
#include <QSignalSpy>

#include "../src/bootstrap/daemonservicebootstraprunner.h"

class DaemonServiceBootstrapRunnerTest : public QObject
{
    Q_OBJECT

private slots:
    void readyFlow_contract()
    {
        int checks = 0;
        QList<int> spawnAttempts;

        Slm::Bootstrap::DaemonServiceBootstrapRunner runner(
            QStringLiteral("org.test.Service"),
            QStringLiteral("local-bin"),
            QStringLiteral("fallback-bin"),
            QStringLiteral("test"),
            [&](const QString &) {
                ++checks;
                return checks >= 4; // ready at attempt 4
            },
            [&](const QString &, const QString &) {
                spawnAttempts.push_back(checks);
                return true;
            },
            this,
            1);

        QSignalSpy readySpy(&runner,
                            SIGNAL(serviceReady(QString,int)));
        QSignalSpy giveUpSpy(&runner,
                             SIGNAL(gaveUp(QString,int)));
        QVERIFY(readySpy.isValid());
        QVERIFY(giveUpSpy.isValid());

        runner.start();
        QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, 500);
        QCOMPARE(giveUpSpy.count(), 0);
        QVERIFY(!runner.isRunning());
        QVERIFY(runner.isReady());
        QVERIFY(!runner.hasGivenUp());
        QCOMPARE(runner.readyAttempt(), 4);
        QCOMPARE(runner.gaveUpAttempt(), 0);
        QCOMPARE(runner.attemptCount(), 4);
        QCOMPARE(runner.spawnAttemptCount(), 2);
        QCOMPARE(runner.spawnStartedCount(), 2);
        QCOMPARE(runner.lastSpawnAttempt(), 3);

        QCOMPARE(spawnAttempts.size(), 2);
        QCOMPARE(spawnAttempts.at(0), 1);
        QCOMPARE(spawnAttempts.at(1), 3);
    }

    void giveUpFlow_contract()
    {
        int checks = 0;
        int spawnCount = 0;

        Slm::Bootstrap::DaemonServiceBootstrapRunner runner(
            QStringLiteral("org.test.NeverReady"),
            QStringLiteral("local-bin"),
            QStringLiteral("fallback-bin"),
            QStringLiteral("test"),
            [&](const QString &) {
                ++checks;
                return false;
            },
            [&](const QString &, const QString &) {
                ++spawnCount;
                return true;
            },
            this,
            1);

        QSignalSpy readySpy(&runner,
                            SIGNAL(serviceReady(QString,int)));
        QSignalSpy giveUpSpy(&runner,
                             SIGNAL(gaveUp(QString,int)));
        QVERIFY(readySpy.isValid());
        QVERIFY(giveUpSpy.isValid());

        runner.start();
        QTRY_COMPARE_WITH_TIMEOUT(giveUpSpy.count(), 1, 1000);
        QCOMPARE(readySpy.count(), 0);
        QVERIFY(!runner.isRunning());
        QVERIFY(!runner.isReady());
        QVERIFY(runner.hasGivenUp());
        QCOMPARE(runner.readyAttempt(), 0);
        QCOMPARE(runner.gaveUpAttempt(), Slm::Bootstrap::DaemonServiceBootstrap::kMaxAttempts);
        QCOMPARE(runner.attemptCount(), Slm::Bootstrap::DaemonServiceBootstrap::kMaxAttempts);
        QCOMPARE(runner.spawnAttemptCount(), 5);
        QCOMPARE(runner.spawnStartedCount(), 5);
        QCOMPARE(runner.lastSpawnAttempt(), Slm::Bootstrap::DaemonServiceBootstrap::kMaxAttempts);

        QCOMPARE(spawnCount, 5); // attempts 1,3,6,9,12
        QCOMPARE(checks, Slm::Bootstrap::DaemonServiceBootstrap::kMaxAttempts);
    }
};

QTEST_MAIN(DaemonServiceBootstrapRunnerTest)
#include "daemonservicebootstraprunner_test.moc"
