#include <QtTest/QtTest>

#include "../src/bootstrap/daemonservicebootstrap.h"

class DaemonServiceBootstrapTest : public QObject
{
    Q_OBJECT

private slots:
    void shouldSpawnOnAttempt_contract()
    {
        using Slm::Bootstrap::DaemonServiceBootstrap;
        QVERIFY(!DaemonServiceBootstrap::shouldSpawnOnAttempt(0));
        QVERIFY(DaemonServiceBootstrap::shouldSpawnOnAttempt(1));
        QVERIFY(!DaemonServiceBootstrap::shouldSpawnOnAttempt(2));
        QVERIFY(DaemonServiceBootstrap::shouldSpawnOnAttempt(3));
        QVERIFY(!DaemonServiceBootstrap::shouldSpawnOnAttempt(4));
        QVERIFY(!DaemonServiceBootstrap::shouldSpawnOnAttempt(5));
        QVERIFY(DaemonServiceBootstrap::shouldSpawnOnAttempt(6));
    }

    void shouldStopRetry_contract()
    {
        using Slm::Bootstrap::DaemonServiceBootstrap;
        QVERIFY(DaemonServiceBootstrap::shouldStopRetry(true, 1));
        QVERIFY(!DaemonServiceBootstrap::shouldStopRetry(false, 1));
        QVERIFY(!DaemonServiceBootstrap::shouldStopRetry(false,
                                                         DaemonServiceBootstrap::kMaxAttempts - 1));
        QVERIFY(DaemonServiceBootstrap::shouldStopRetry(false,
                                                        DaemonServiceBootstrap::kMaxAttempts));
        QVERIFY(DaemonServiceBootstrap::shouldStopRetry(false,
                                                        DaemonServiceBootstrap::kMaxAttempts + 1));
    }

    void retryRecoverySimulation_contract()
    {
        using Slm::Bootstrap::DaemonServiceBootstrap;

        auto simulate = [](int readyAtAttempt) -> QPair<int, int> {
            int spawnCount = 0;
            int stoppedAt = -1;
            for (int attempt = 1; attempt <= DaemonServiceBootstrap::kMaxAttempts; ++attempt) {
                if (DaemonServiceBootstrap::shouldSpawnOnAttempt(attempt)) {
                    ++spawnCount;
                }
                const bool ready = (readyAtAttempt > 0 && attempt >= readyAtAttempt);
                if (DaemonServiceBootstrap::shouldStopRetry(ready, attempt)) {
                    stoppedAt = attempt;
                    break;
                }
            }
            return {spawnCount, stoppedAt};
        };

        // Recovers fast: only first spawn should happen, then stop.
        {
            const auto [spawns, stop] = simulate(2);
            QCOMPARE(spawns, 1);
            QCOMPARE(stop, 2);
        }

        // Recovers later: spawns on attempt 1 and 3, then stop at 4.
        {
            const auto [spawns, stop] = simulate(4);
            QCOMPARE(spawns, 2);
            QCOMPARE(stop, 4);
        }

        // Never recovers: stop at max attempts with expected spawn cadence.
        {
            const auto [spawns, stop] = simulate(-1);
            QCOMPARE(stop, DaemonServiceBootstrap::kMaxAttempts);
            QCOMPARE(spawns, 5); // attempts 1,3,6,9,12
        }
    }
};

QTEST_MAIN(DaemonServiceBootstrapTest)
#include "daemonservicebootstrap_test.moc"
