#include <QtTest/QtTest>
#include <QVariantMap>

#include "../src/daemon/desktopd/daemonhealthmonitor.h"

class DaemonHealthMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void snapshot_contractShape()
    {
        DaemonHealthMonitor monitor;
        QTest::qWait(10);

        const QVariantMap snapshot = monitor.snapshot();
        QVERIFY(snapshot.contains(QStringLiteral("fileOperations")));
        QVERIFY(snapshot.contains(QStringLiteral("devices")));
        QVERIFY(snapshot.contains(QStringLiteral("baseDelayMs")));
        QVERIFY(snapshot.contains(QStringLiteral("maxDelayMs")));

        const int baseDelay = snapshot.value(QStringLiteral("baseDelayMs")).toInt();
        const int maxDelay = snapshot.value(QStringLiteral("maxDelayMs")).toInt();
        QVERIFY(baseDelay > 0);
        QVERIFY(maxDelay >= baseDelay);

        auto checkPeerShape = [](const QVariantMap &peer) {
            QVERIFY(peer.contains(QStringLiteral("service")));
            QVERIFY(peer.contains(QStringLiteral("path")));
            QVERIFY(peer.contains(QStringLiteral("iface")));
            QVERIFY(peer.contains(QStringLiteral("registered")));
            QVERIFY(peer.contains(QStringLiteral("failures")));
            QVERIFY(peer.contains(QStringLiteral("lastCheckMs")));
            QVERIFY(peer.contains(QStringLiteral("lastOkMs")));
            QVERIFY(peer.contains(QStringLiteral("lastError")));
            QVERIFY(peer.contains(QStringLiteral("nextCheckInMs")));
            QVERIFY(!peer.value(QStringLiteral("service")).toString().isEmpty());
            QVERIFY(!peer.value(QStringLiteral("path")).toString().isEmpty());
            QVERIFY(!peer.value(QStringLiteral("iface")).toString().isEmpty());
        };

        QVERIFY(snapshot.value(QStringLiteral("fileOperations")).canConvert<QVariantMap>());
        QVERIFY(snapshot.value(QStringLiteral("devices")).canConvert<QVariantMap>());
        checkPeerShape(snapshot.value(QStringLiteral("fileOperations")).toMap());
        checkPeerShape(snapshot.value(QStringLiteral("devices")).toMap());
    }

    void snapshot_scheduler_progresses()
    {
        DaemonHealthMonitor monitor;
        QTest::qWait(20);

        const QVariantMap snap1 = monitor.snapshot();
        const QVariantMap fileOps1 = snap1.value(QStringLiteral("fileOperations")).toMap();
        const QVariantMap devices1 = snap1.value(QStringLiteral("devices")).toMap();
        const int fileOpsT1 = fileOps1.value(QStringLiteral("nextCheckInMs")).toInt();
        const int devicesT1 = devices1.value(QStringLiteral("nextCheckInMs")).toInt();

        QTest::qWait(150);

        const QVariantMap snap2 = monitor.snapshot();
        const QVariantMap fileOps2 = snap2.value(QStringLiteral("fileOperations")).toMap();
        const QVariantMap devices2 = snap2.value(QStringLiteral("devices")).toMap();
        const int fileOpsT2 = fileOps2.value(QStringLiteral("nextCheckInMs")).toInt();
        const int devicesT2 = devices2.value(QStringLiteral("nextCheckInMs")).toInt();

        QVERIFY(fileOpsT1 >= -1);
        QVERIFY(fileOpsT2 >= -1);
        QVERIFY(devicesT1 >= -1);
        QVERIFY(devicesT2 >= -1);

        const bool fileOpsChanged = (fileOpsT1 >= 0 && fileOpsT2 >= 0 && fileOpsT1 != fileOpsT2);
        const bool devicesChanged = (devicesT1 >= 0 && devicesT2 >= 0 && devicesT1 != devicesT2);
        QVERIFY2(fileOpsChanged || devicesChanged,
                 "watchdog scheduler did not show progress between snapshots");
    }

    void snapshot_peerState_contractAndMonotonic()
    {
        DaemonHealthMonitor monitor;
        QTest::qWait(30);

        const auto readPeer = [&monitor](const QString &key) {
            return monitor.snapshot().value(key).toMap();
        };

        const QVariantMap fileOps1 = readPeer(QStringLiteral("fileOperations"));
        const QVariantMap devices1 = readPeer(QStringLiteral("devices"));
        QTest::qWait(120);
        const QVariantMap fileOps2 = readPeer(QStringLiteral("fileOperations"));
        const QVariantMap devices2 = readPeer(QStringLiteral("devices"));

        auto assertPeerTypes = [](const QVariantMap &peer) {
            QVERIFY(peer.value(QStringLiteral("registered")).canConvert<bool>());
            QVERIFY(peer.value(QStringLiteral("failures")).canConvert<int>());
            QVERIFY(peer.value(QStringLiteral("lastCheckMs")).canConvert<qlonglong>());
            QVERIFY(peer.value(QStringLiteral("lastOkMs")).canConvert<qlonglong>());
            QVERIFY(peer.value(QStringLiteral("lastError")).canConvert<QString>());
            QVERIFY(peer.value(QStringLiteral("nextCheckInMs")).canConvert<int>());
        };

        assertPeerTypes(fileOps1);
        assertPeerTypes(fileOps2);
        assertPeerTypes(devices1);
        assertPeerTypes(devices2);

        auto assertMonotonic = [](const QVariantMap &before, const QVariantMap &after) {
            const qlonglong c1 = before.value(QStringLiteral("lastCheckMs")).toLongLong();
            const qlonglong c2 = after.value(QStringLiteral("lastCheckMs")).toLongLong();
            QVERIFY2(c2 >= c1, "lastCheckMs must be monotonic");
        };

        assertMonotonic(fileOps1, fileOps2);
        assertMonotonic(devices1, devices2);
    }

    void snapshot_lastError_whenPeerMissing()
    {
        DaemonHealthMonitor monitor;

        QVariantMap fileOps;
        QVariantMap devices;
        bool sawMissingPeerAfterCheck = false;

        for (int i = 0; i < 30; ++i) {
            const QVariantMap snap = monitor.snapshot();
            fileOps = snap.value(QStringLiteral("fileOperations")).toMap();
            devices = snap.value(QStringLiteral("devices")).toMap();
            const bool fileMissingAfterCheck =
                !fileOps.value(QStringLiteral("registered")).toBool() &&
                fileOps.value(QStringLiteral("lastCheckMs")).toLongLong() > 0;
            const bool devicesMissingAfterCheck =
                !devices.value(QStringLiteral("registered")).toBool() &&
                devices.value(QStringLiteral("lastCheckMs")).toLongLong() > 0;
            if (fileMissingAfterCheck || devicesMissingAfterCheck) {
                sawMissingPeerAfterCheck = true;
                break;
            }
            QTest::qWait(100);
        }

        if (!sawMissingPeerAfterCheck) {
            QSKIP("No missing peer observed after a completed health check in this environment");
        }

        const auto assertMissingPeerError = [](const QVariantMap &peer) {
            const bool missingAfterCheck =
                !peer.value(QStringLiteral("registered")).toBool() &&
                peer.value(QStringLiteral("lastCheckMs")).toLongLong() > 0;
            if (!missingAfterCheck) {
                return;
            }
            const int failures = peer.value(QStringLiteral("failures")).toInt();
            const QString lastError = peer.value(QStringLiteral("lastError")).toString();
            QVERIFY2(failures > 0, "missing peer must have failures > 0");
            QVERIFY2(!lastError.isEmpty(), "missing peer must expose non-empty lastError");
        };

        assertMissingPeerError(fileOps);
        assertMissingPeerError(devices);
    }
};

QTEST_MAIN(DaemonHealthMonitorTest)
#include "daemonhealthmonitor_test.moc"
