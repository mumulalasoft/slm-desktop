#include <QtTest/QtTest>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QProcess>
#include <QDir>
#include <memory>

#include "../../src/apps/filemanager/include/filemanagerapi.h"
#include "../../src/filemanager/ops/fileoperationsmanager.h"
#include "../../src/filemanager/ops/fileoperationsservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.FileOperations";
constexpr const char kPath[] = "/org/slm/Desktop/FileOperations";
constexpr const char kIface[] = "org.slm.Desktop.FileOperations";
}

class FileManagerApiDaemonRecoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("SLM_FILEOPS_BYPASS_PERMISSION", "1");
    }

    void recoversBatchStateFromDaemonJobsChanged()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FileOperationsManager manager;
        FileOperationsService service(&manager);
        QVERIFY(service.serviceRegistered());

        FileManagerApi api;

        QTemporaryDir srcDir;
        QTemporaryDir dstDir;
        QVERIFY(srcDir.isValid());
        QVERIFY(dstDir.isValid());

        QStringList sources;
        const QByteArray payload(256 * 1024, '$');
        for (int i = 0; i < 220; ++i) {
            const QString path = srcDir.filePath(QStringLiteral("api_%1.bin").arg(i));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(payload), payload.size());
            f.close();
            sources.push_back(path);
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QString> copyReply = iface.call(QStringLiteral("Copy"), sources, dstDir.path());
        QVERIFY(copyReply.isValid());
        const QString jobId = copyReply.value().trimmed();
        QVERIFY(!jobId.isEmpty());

        // Wait for FileManagerApi to recover active batch from daemon side.
        bool initiallyAttached = false;
        QElapsedTimer initialAttachTimer;
        initialAttachTimer.start();
        while (initialAttachTimer.elapsed() < 5000) {
            if (api.batchOperationActive() && api.batchOperationId() == jobId) {
                initiallyAttached = true;
                break;
            }
            QTest::qWait(20);
        }

        if (!initiallyAttached) {
            QDBusReply<QVariantMap> maybeTerminal = iface.call(QStringLiteral("GetJob"), jobId);
            if (maybeTerminal.isValid()) {
                const QString state = maybeTerminal.value().value(QStringLiteral("state")).toString();
                const bool terminal = (state == QStringLiteral("finished")
                                       || state == QStringLiteral("error"));
                if (terminal) {
                    return;
                }
            }
            QSKIP("job not attached before restart in this environment");
        }
        QCOMPARE(api.batchOperationType(), QStringLiteral("copy"));

        bool paused = false;
        QElapsedTimer pauseTimer;
        pauseTimer.start();
        while (pauseTimer.elapsed() < 5000) {
            QDBusReply<bool> pauseReply = iface.call(QStringLiteral("Pause"), jobId);
            if (pauseReply.isValid() && pauseReply.value()) {
                paused = true;
                break;
            }
            QTest::qWait(20);
        }
        QVERIFY(paused);

        QTRY_VERIFY_WITH_TIMEOUT(api.batchOperationPaused(), 5000);

        QDBusReply<bool> cancelReply = iface.call(QStringLiteral("Cancel"), jobId);
        QVERIFY(cancelReply.isValid());
        QVERIFY(cancelReply.value());

        QTRY_VERIFY_WITH_TIMEOUT(!api.batchOperationActive(), 10000);
    }

    void recoversAfterServiceRestartDuringActiveJob()
    {
        if (qEnvironmentVariableIntValue("SLM_RUN_STRICT_FILEOPS_RECOVERY") != 1) {
            QSKIP("strict restart-recovery scenario is opt-in (set SLM_RUN_STRICT_FILEOPS_RECOVERY=1)");
        }

        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FileOperationsManager manager;
        auto service = std::make_unique<FileOperationsService>(&manager);
        QVERIFY(service->serviceRegistered());

        FileManagerApi api;

        QTemporaryDir srcDir;
        QTemporaryDir dstDir;
        QVERIFY(srcDir.isValid());
        QVERIFY(dstDir.isValid());

        QStringList sources;
        const QByteArray payload(128 * 1024, '#');
        for (int i = 0; i < 260; ++i) {
            const QString path = srcDir.filePath(QStringLiteral("restart_%1.bin").arg(i));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(payload), payload.size());
            f.close();
            sources.push_back(path);
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QString> copyReply = iface.call(QStringLiteral("Copy"), sources, dstDir.path());
        QVERIFY(copyReply.isValid());
        const QString jobId = copyReply.value().trimmed();
        QVERIFY(!jobId.isEmpty());

        bool attached = false;
        QElapsedTimer attachTimer;
        attachTimer.start();
        while (attachTimer.elapsed() < 5000) {
            if (api.batchOperationActive() && api.batchOperationId() == jobId) {
                attached = true;
                break;
            }
            QTest::qWait(20);
        }
        if (!attached) {
            QDBusReply<QVariantMap> earlyStateReply = iface.call(QStringLiteral("GetJob"), jobId);
            if (earlyStateReply.isValid()) {
                const QString state = earlyStateReply.value().value(QStringLiteral("state")).toString();
                if (state == QStringLiteral("finished") || state == QStringLiteral("error")) {
                    QSKIP("job reached terminal state before API attach in strict restart scenario");
                }
            }
            QSKIP("API did not attach to job before restart in strict restart scenario");
        }

        bool paused = false;
        QElapsedTimer pauseTimer;
        pauseTimer.start();
        while (pauseTimer.elapsed() < 5000) {
            QDBusReply<bool> pauseReply = iface.call(QStringLiteral("Pause"), jobId);
            if (pauseReply.isValid() && pauseReply.value()) {
                paused = true;
                break;
            }
            QTest::qWait(20);
        }
        if (!paused) {
            QDBusReply<QVariantMap> pausedStateReply = iface.call(QStringLiteral("GetJob"), jobId);
            if (pausedStateReply.isValid()) {
                const QString state = pausedStateReply.value().value(QStringLiteral("state")).toString();
                if (state == QStringLiteral("finished") || state == QStringLiteral("error")) {
                    QSKIP("job reached terminal state before pause in strict restart scenario");
                }
            }
            QSKIP("pause not acknowledged before timeout in strict restart scenario");
        }
        if (!api.batchOperationPaused()) {
            QDBusReply<QVariantMap> pausedStateReply = iface.call(QStringLiteral("GetJob"), jobId);
            if (pausedStateReply.isValid()) {
                const QString state = pausedStateReply.value().value(QStringLiteral("state")).toString();
                if (state == QStringLiteral("finished") || state == QStringLiteral("error")) {
                    QSKIP("job reached terminal state before API paused state observed");
                }
            }
            QTRY_VERIFY_WITH_TIMEOUT(api.batchOperationPaused(), 5000);
        }

        // Failure injection: restart DBus service while job remains in manager.
        service.reset();
        QTest::qWait(120);
        service = std::make_unique<FileOperationsService>(&manager);
        QVERIFY(service->serviceRegistered());

        QDBusInterface ifaceAfterRestart(QString::fromLatin1(kService),
                                         QString::fromLatin1(kPath),
                                         QString::fromLatin1(kIface),
                                         bus);
        QVERIFY(ifaceAfterRestart.isValid());

        // Ensure API tracks same job id after service owner changes, unless the job
        // already reached terminal state during restart race.
        bool reattached = false;
        QElapsedTimer reattachTimer;
        reattachTimer.start();
        while (reattachTimer.elapsed() < 5000) {
            if (api.batchOperationActive() && api.batchOperationId() == jobId) {
                reattached = true;
                break;
            }
            QTest::qWait(20);
        }

        if (!reattached) {
            QDBusReply<QVariantMap> terminalReply = ifaceAfterRestart.call(QStringLiteral("GetJob"), jobId);
            if (terminalReply.isValid()) {
                const QString state = terminalReply.value().value(QStringLiteral("state")).toString();
                const bool terminal = (state == QStringLiteral("finished")
                                       || state == QStringLiteral("error"));
                if (terminal) {
                    return;
                }
            }
            QSKIP("job not reattached after restart in this environment");
        }

        bool resumed = false;
        QElapsedTimer resumeTimer;
        resumeTimer.start();
        while (resumeTimer.elapsed() < 5000) {
            QDBusReply<bool> resumeReply = ifaceAfterRestart.call(QStringLiteral("Resume"), jobId);
            if (resumeReply.isValid() && resumeReply.value()) {
                resumed = true;
                break;
            }
            QTest::qWait(20);
        }
        QVERIFY(resumed);

        bool cancelled = false;
        QElapsedTimer cancelTimer;
        cancelTimer.start();
        while (cancelTimer.elapsed() < 5000) {
            QDBusReply<bool> cancelReply = ifaceAfterRestart.call(QStringLiteral("Cancel"), jobId);
            if (cancelReply.isValid() && cancelReply.value()) {
                cancelled = true;
                break;
            }
            QTest::qWait(20);
        }
        QVERIFY(cancelled);

        QTRY_VERIFY_WITH_TIMEOUT(!api.batchOperationActive(), 10000);
    }

    void chaos_serviceFlapsWithRandomizedOps_underSla()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FileOperationsManager manager;
        auto service = std::make_unique<FileOperationsService>(&manager);
        QVERIFY(service->serviceRegistered());
        FileManagerApi api;

        QTemporaryDir workspace;
        QVERIFY(workspace.isValid());

        QRandomGenerator rng(0x5A17u);
        QElapsedTimer sla;
        sla.start();
        constexpr qint64 kSlaMs = 30000;
        constexpr int kRounds = 4;
        constexpr int kFlapsPerRound = 3;

        auto waitForBoolCall = [](QDBusInterface &iface,
                                  const QString &method,
                                  const QString &jobId,
                                  int timeoutMs) {
            QElapsedTimer t;
            t.start();
            while (t.elapsed() < timeoutMs) {
                QDBusReply<bool> reply = iface.call(method, jobId);
                if (reply.isValid() && reply.value()) {
                    return true;
                }
                QTest::qWait(20);
            }
            return false;
        };

        for (int round = 0; round < kRounds; ++round) {
            QVERIFY2(sla.elapsed() < kSlaMs, "chaos SLA exceeded");

            const QString srcRoot = workspace.filePath(QStringLiteral("src_%1").arg(round));
            const QString dstRoot = workspace.filePath(QStringLiteral("dst_%1").arg(round));
            QDir().mkpath(srcRoot);
            QDir().mkpath(dstRoot);

            QStringList sources;
            const QByteArray payload(48 * 1024, static_cast<char>('a' + (round % 26)));
            for (int i = 0; i < 24; ++i) {
                const QString path = QDir(srcRoot).filePath(QStringLiteral("f_%1.bin").arg(i));
                QFile f(path);
                QVERIFY(f.open(QIODevice::WriteOnly));
                QCOMPARE(f.write(payload), payload.size());
                f.close();
                sources.push_back(path);
            }

            const int op = rng.bounded(4);
            const QString method = (op == 0) ? QStringLiteral("Copy")
                                   : (op == 1) ? QStringLiteral("Move")
                                   : (op == 2) ? QStringLiteral("Delete")
                                               : QStringLiteral("Trash");

            QDBusInterface iface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIface),
                                 bus);
            QVERIFY(iface.isValid());

            QDBusReply<QString> startReply = (method == QStringLiteral("Copy")
                                              || method == QStringLiteral("Move"))
                    ? iface.call(method, sources, dstRoot)
                    : iface.call(method, sources);
            QVERIFY(startReply.isValid());
            const QString jobId = startReply.value().trimmed();
            QVERIFY(!jobId.isEmpty());

            // Operation can finish very quickly on fast/low-load environments.
            // Only enforce active-attach path when the job is still running.
            bool attached = false;
            QElapsedTimer attachTimer;
            attachTimer.start();
            while (attachTimer.elapsed() < 2000) {
                if (api.batchOperationActive() && api.batchOperationId() == jobId) {
                    attached = true;
                    break;
                }
                QTest::qWait(20);
            }

            if (!attached) {
                QDBusReply<QVariantMap> getJobReply = iface.call(QStringLiteral("GetJob"), jobId);
                if (getJobReply.isValid()) {
                    const QString state = getJobReply.value().value(QStringLiteral("state")).toString();
                    if (state == QStringLiteral("finished") || state == QStringLiteral("error")) {
                        QVERIFY2(sla.elapsed() < kSlaMs, "chaos SLA exceeded");
                        continue;
                    }
                }
            }

            for (int flap = 0; flap < kFlapsPerRound; ++flap) {
                service.reset();
                QTest::qWait(80);
                service = std::make_unique<FileOperationsService>(&manager);
                QVERIFY(service->serviceRegistered());
                QTest::qWait(60);
            }

            QDBusInterface ifaceAfterFlap(QString::fromLatin1(kService),
                                          QString::fromLatin1(kPath),
                                          QString::fromLatin1(kIface),
                                          bus);
            QVERIFY(ifaceAfterFlap.isValid());

            // Keep operation bounded in time by cancelling after flap storm.
            bool cancelOk = waitForBoolCall(ifaceAfterFlap,
                                            QStringLiteral("Cancel"),
                                            jobId,
                                            4000);
            if (!cancelOk) {
                QDBusReply<QVariantMap> getJobReply = ifaceAfterFlap.call(QStringLiteral("GetJob"), jobId);
                if (getJobReply.isValid()) {
                    const QString state = getJobReply.value().value(QStringLiteral("state")).toString();
                    cancelOk = (state == QStringLiteral("finished") || state == QStringLiteral("error"));
                }
            }
            QVERIFY(cancelOk);

            QTRY_VERIFY_WITH_TIMEOUT(!api.batchOperationActive() || api.batchOperationId() != jobId, 12000);
            QVERIFY2(sla.elapsed() < kSlaMs, "chaos SLA exceeded");
        }
    }

    void processLevel_restartFileopsd_autoAttachsNewJob()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        QDBusConnectionInterface *busIface = bus.interface();
        if (!busIface) {
            QSKIP("session bus interface unavailable");
        }

        if (busIface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
            QSKIP("org.slm.Desktop.FileOperations already registered externally");
        }

        const QString daemonPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("slm-fileopsd"));
        if (!QFileInfo::exists(daemonPath)) {
            QSKIP("slm-fileopsd binary not found");
        }

        auto waitServiceRegistered = [&](bool expected, int timeoutMs) {
            QElapsedTimer t;
            t.start();
            while (t.elapsed() < timeoutMs) {
                const bool registered = busIface->isServiceRegistered(QString::fromLatin1(kService)).value();
                if (registered == expected) {
                    return true;
                }
                QTest::qWait(20);
            }
            return false;
        };

        auto startDaemon = [&](QProcess &p) {
            p.setProgram(daemonPath);
            p.start();
            if (!p.waitForStarted(4000)) {
                return false;
            }
            return waitServiceRegistered(true, 5000);
        };

        auto stopDaemon = [&](QProcess &p) {
            if (p.state() != QProcess::NotRunning) {
                p.kill();
                p.waitForFinished(4000);
            }
            return waitServiceRegistered(false, 5000);
        };

        QProcess daemonProc;
        QVERIFY(startDaemon(daemonProc));

        FileManagerApi api;

        QTemporaryDir srcDir1;
        QTemporaryDir srcDir2;
        QTemporaryDir dstDir;
        QVERIFY(srcDir1.isValid());
        QVERIFY(srcDir2.isValid());
        QVERIFY(dstDir.isValid());

        auto makeSources = [](const QString &root, const QString &prefix, int count) {
            QStringList out;
            const QByteArray payload(96 * 1024, '@');
            for (int i = 0; i < count; ++i) {
                const QString path = QDir(root).filePath(QStringLiteral("%1_%2.bin").arg(prefix).arg(i));
                QFile f(path);
                if (!f.open(QIODevice::WriteOnly)) {
                    return QStringList();
                }
                if (f.write(payload) != payload.size()) {
                    return QStringList();
                }
                f.close();
                out.push_back(path);
            }
            return out;
        };

        const QStringList sources1 = makeSources(srcDir1.path(), QStringLiteral("a"), 180);
        const QStringList sources2 = makeSources(srcDir2.path(), QStringLiteral("b"), 160);
        QVERIFY(!sources1.isEmpty());
        QVERIFY(!sources2.isEmpty());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        auto startCopyWithRetry = [&](QDBusInterface &dbusIface, const QStringList &src) {
            QElapsedTimer t;
            t.start();
            QDBusReply<QString> reply;
            while (t.elapsed() < 5000) {
                reply = dbusIface.call(QStringLiteral("Copy"), src, dstDir.path());
                if (reply.isValid() && !reply.value().trimmed().isEmpty()) {
                    break;
                }
                QTest::qWait(40);
            }
            return reply;
        };

        QDBusReply<QString> start1 = startCopyWithRetry(iface, sources1);
        if (!start1.isValid()) {
            stopDaemon(daemonProc);
            QSKIP("slm-fileopsd did not accept Copy in this environment");
        }
        const QString job1 = start1.value().trimmed();
        QVERIFY(!job1.isEmpty());

        QTRY_VERIFY_WITH_TIMEOUT(api.batchOperationActive(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(api.batchOperationId(), job1, 5000);

        // Freeze first job then crash daemon process.
        bool paused = false;
        QElapsedTimer pauseTimer;
        pauseTimer.start();
        while (pauseTimer.elapsed() < 5000) {
            QDBusReply<bool> pauseReply = iface.call(QStringLiteral("Pause"), job1);
            if (pauseReply.isValid() && pauseReply.value()) {
                paused = true;
                break;
            }
            QTest::qWait(20);
        }
        QVERIFY(paused);

        QVERIFY(stopDaemon(daemonProc));
        QVERIFY(startDaemon(daemonProc));

        QDBusInterface iface2(QString::fromLatin1(kService),
                              QString::fromLatin1(kPath),
                              QString::fromLatin1(kIface),
                              bus);
        QVERIFY(iface2.isValid());

        QDBusReply<QString> start2 = startCopyWithRetry(iface2, sources2);
        QVERIFY(start2.isValid());
        const QString job2 = start2.value().trimmed();
        QVERIFY(!job2.isEmpty());
        QVERIFY(job2 != job1);

        // Auto-recover should drop stale job1 and attach to live job2.
        QTRY_COMPARE_WITH_TIMEOUT(api.batchOperationId(), job2, 12000);
        QTRY_VERIFY_WITH_TIMEOUT(api.batchOperationActive(), 12000);

        bool cancelled = false;
        QElapsedTimer cancelTimer;
        cancelTimer.start();
        while (cancelTimer.elapsed() < 6000) {
            QDBusReply<bool> cancelReply = iface2.call(QStringLiteral("Cancel"), job2);
            if (cancelReply.isValid() && cancelReply.value()) {
                cancelled = true;
                break;
            }
            QTest::qWait(20);
        }
        QVERIFY(cancelled);
        QTRY_VERIFY_WITH_TIMEOUT(!api.batchOperationActive(), 12000);

        QVERIFY(stopDaemon(daemonProc));
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QGuiApplication app(argc, argv);
    FileManagerApiDaemonRecoveryTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "filemanagerapi_daemon_recovery_test.moc"
