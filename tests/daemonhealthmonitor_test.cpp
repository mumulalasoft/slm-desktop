#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QVariantMap>
#include <memory>

#include "../src/daemon/desktopd/daemonhealthmonitor.h"

class DaemonHealthMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_tmpDir.reset(new QTemporaryDir);
        QVERIFY(m_tmpDir->isValid());
        m_timelinePath = m_tmpDir->path() + QStringLiteral("/daemon-health-timeline.json");
        qputenv("SLM_DAEMON_HEALTH_TIMELINE_FILE", m_timelinePath.toUtf8());
        qputenv("SLM_DAEMON_HEALTH_TIMELINE_LIMIT", "64");
    }

    void cleanup()
    {
        qunsetenv("SLM_DAEMON_HEALTH_TIMELINE_FILE");
        qunsetenv("SLM_DAEMON_HEALTH_TIMELINE_LIMIT");
        m_tmpDir.reset();
        m_timelinePath.clear();
    }

    void snapshot_contractShape()
    {
        DaemonHealthMonitor monitor;
        QTest::qWait(10);

        const QVariantMap snapshot = monitor.snapshot();
        QVERIFY(snapshot.contains(QStringLiteral("fileOperations")));
        QVERIFY(snapshot.contains(QStringLiteral("devices")));
        QVERIFY(snapshot.contains(QStringLiteral("baseDelayMs")));
        QVERIFY(snapshot.contains(QStringLiteral("maxDelayMs")));
        QVERIFY(snapshot.contains(QStringLiteral("degraded")));
        QVERIFY(snapshot.contains(QStringLiteral("reasonCodes")));
        QVERIFY(snapshot.contains(QStringLiteral("timeline")));
        QVERIFY(snapshot.contains(QStringLiteral("timelineSize")));
        QVERIFY(snapshot.contains(QStringLiteral("timelineFile")));

        const int baseDelay = snapshot.value(QStringLiteral("baseDelayMs")).toInt();
        const int maxDelay = snapshot.value(QStringLiteral("maxDelayMs")).toInt();
        QVERIFY(baseDelay > 0);
        QVERIFY(maxDelay >= baseDelay);
        QVERIFY(snapshot.value(QStringLiteral("timeline")).canConvert<QVariantList>());
        QVERIFY(snapshot.value(QStringLiteral("timelineSize")).toInt() >= 1);
        QCOMPARE(snapshot.value(QStringLiteral("timelineFile")).toString(), m_timelinePath);

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

    void timeline_persistsAcrossInstances()
    {
        {
            DaemonHealthMonitor first;
            QTest::qWait(20);
            const QVariantMap snap = first.snapshot();
            const QVariantList timeline = snap.value(QStringLiteral("timeline")).toList();
            QVERIFY(!timeline.isEmpty());
            const QVariantMap last = timeline.constLast().toMap();
            QCOMPARE(last.value(QStringLiteral("code")).toString(), QStringLiteral("monitor-started"));
        }

        DaemonHealthMonitor second;
        QTest::qWait(20);
        const QVariantMap snap2 = second.snapshot();
        const QVariantList timeline2 = snap2.value(QStringLiteral("timeline")).toList();
        QVERIFY2(timeline2.size() >= 2, "timeline should retain previous entries and append new startup event");
    }

private:
    std::unique_ptr<QTemporaryDir> m_tmpDir;
    QString m_timelinePath;
};

QTEST_MAIN(DaemonHealthMonitorTest)
#include "daemonhealthmonitor_test.moc"
