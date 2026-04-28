#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QTemporaryDir>
#include <QVariantMap>
#include <memory>

#include "../src/daemon/desktopd/daemonhealthmonitor.h"

class FakeFileOperationsPeer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.FileOperations")

public slots:
    QVariantMap Ping() const
    {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("service"), QStringLiteral("org.slm.Desktop.FileOperations")},
        };
    }
};

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
        QVERIFY(snapshot.contains(QStringLiteral("criticalReasonCodes")));
        QVERIFY(snapshot.contains(QStringLiteral("optionalReasonCodes")));
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
            QVERIFY(peer.contains(QStringLiteral("critical")));
            QVERIFY(peer.contains(QStringLiteral("registered")));
            QVERIFY(peer.contains(QStringLiteral("failures")));
            QVERIFY(peer.contains(QStringLiteral("lastCheckMs")));
            QVERIFY(peer.contains(QStringLiteral("lastOkMs")));
            QVERIFY(peer.contains(QStringLiteral("lastError")));
            QVERIFY(peer.contains(QStringLiteral("nextCheckInMs")));
            QVERIFY(!peer.value(QStringLiteral("service")).toString().isEmpty());
            QVERIFY(!peer.value(QStringLiteral("path")).toString().isEmpty());
            QVERIFY(!peer.value(QStringLiteral("iface")).toString().isEmpty());
            QVERIFY(peer.value(QStringLiteral("critical")).canConvert<bool>());
        };

        QVERIFY(snapshot.value(QStringLiteral("fileOperations")).canConvert<QVariantMap>());
        QVERIFY(snapshot.value(QStringLiteral("devices")).canConvert<QVariantMap>());
        const QVariantMap fileOps = snapshot.value(QStringLiteral("fileOperations")).toMap();
        const QVariantMap devices = snapshot.value(QStringLiteral("devices")).toMap();
        checkPeerShape(fileOps);
        checkPeerShape(devices);
        QCOMPARE(fileOps.value(QStringLiteral("critical")).toBool(), true);
        QCOMPARE(devices.value(QStringLiteral("critical")).toBool(), false);
        QVERIFY(snapshot.value(QStringLiteral("criticalReasonCodes")).canConvert<QVariantList>());
        QVERIFY(snapshot.value(QStringLiteral("optionalReasonCodes")).canConvert<QVariantList>());
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

    void snapshot_optionalDevicesDoesNotDegradeWhenCriticalPeersHealthy()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected() || !bus.interface()) {
            QSKIP("session bus is not available in this test environment");
        }

        auto serviceRegistered = [&bus](const QString &service) {
            const QDBusReply<bool> reply = bus.interface()->isServiceRegistered(service);
            return reply.isValid() && reply.value();
        };

        if (serviceRegistered(QStringLiteral("org.slm.Desktop.FileOperations"))) {
            QSKIP("file operations service already registered on session bus");
        }
        if (serviceRegistered(QStringLiteral("org.slm.Desktop.Devices"))) {
            QSKIP("devices service already registered on session bus");
        }

        FakeFileOperationsPeer fileOpsPeer;
        if (!bus.registerService(QStringLiteral("org.slm.Desktop.FileOperations"))) {
            QSKIP("could not own org.slm.Desktop.FileOperations on session bus");
        }
        if (!bus.registerObject(QStringLiteral("/org/slm/Desktop/FileOperations"),
                                &fileOpsPeer,
                                QDBusConnection::ExportAllSlots)) {
            bus.unregisterService(QStringLiteral("org.slm.Desktop.FileOperations"));
            QSKIP("could not export fake file operations object on session bus");
        }

        bool observed = false;
        QVariantMap observedSnapshot;
        {
            DaemonHealthMonitor monitor;
            for (int i = 0; i < 40; ++i) {
                QTest::qWait(50);
                const QVariantMap snapshot = monitor.snapshot();
                const QVariantMap fileOps = snapshot.value(QStringLiteral("fileOperations")).toMap();
                const QVariantMap devices = snapshot.value(QStringLiteral("devices")).toMap();
                const bool criticalHealthy =
                    fileOps.value(QStringLiteral("registered")).toBool() &&
                    fileOps.value(QStringLiteral("failures")).toInt() == 0 &&
                    fileOps.value(QStringLiteral("lastOkMs")).toLongLong() > 0;
                const bool optionalMissing =
                    !devices.value(QStringLiteral("registered")).toBool() &&
                    devices.value(QStringLiteral("failures")).toInt() > 0 &&
                    devices.value(QStringLiteral("lastError")).toString()
                        == QStringLiteral("service-not-registered");
                if (criticalHealthy && optionalMissing) {
                    observed = true;
                    observedSnapshot = snapshot;
                    break;
                }
            }
        }

        bus.unregisterObject(QStringLiteral("/org/slm/Desktop/FileOperations"));
        bus.unregisterService(QStringLiteral("org.slm.Desktop.FileOperations"));

        QVERIFY2(observed,
                 "test did not observe healthy critical peer with missing optional devices peer");
        QCOMPARE(observedSnapshot.value(QStringLiteral("degraded")).toBool(), false);

        const QVariantList reasonCodes = observedSnapshot.value(QStringLiteral("reasonCodes")).toList();
        const QVariantList criticalReasonCodes =
            observedSnapshot.value(QStringLiteral("criticalReasonCodes")).toList();
        const QVariantList optionalReasonCodes =
            observedSnapshot.value(QStringLiteral("optionalReasonCodes")).toList();
        QVERIFY(reasonCodes.contains(QStringLiteral("service-not-registered")));
        QVERIFY(criticalReasonCodes.isEmpty());
        QVERIFY(optionalReasonCodes.contains(QStringLiteral("service-not-registered")));
    }

    void timeline_persistsAcrossInstances()
    {
        {
            DaemonHealthMonitor first;
            QTest::qWait(20);
            const QVariantMap snap = first.snapshot();
            const QVariantList timeline = snap.value(QStringLiteral("timeline")).toList();
            QVERIFY(!timeline.isEmpty());
            bool foundStartupEvent = false;
            for (const QVariant &rowValue : timeline) {
                const QVariantMap row = rowValue.toMap();
                if (row.value(QStringLiteral("code")).toString() == QStringLiteral("monitor-started")) {
                    foundStartupEvent = true;
                    break;
                }
            }
            QVERIFY2(foundStartupEvent, "timeline should include a monitor-started event");
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
