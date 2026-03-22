#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>

#include "../../src/filemanager/ops/fileoperationsmanager.h"
#include "../../src/filemanager/ops/fileoperationsservice.h"
#include "../../src/filemanager/ops/fileoperationserrors.h"

class FileopctlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("SLM_FILEOPS_BYPASS_PERMISSION", "1");
    }

    void pauseResumeCancel_viaCliControlsJob()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FileOperationsManager manager;
        FileOperationsService service(&manager);
        QVERIFY(service.serviceRegistered());

        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("fileopctl"));
        QVERIFY2(QFileInfo::exists(toolPath), qPrintable(QStringLiteral("missing tool: %1").arg(toolPath)));

        QTemporaryDir srcDir;
        QTemporaryDir dstDir;
        QVERIFY(srcDir.isValid());
        QVERIFY(dstDir.isValid());

        QStringList sources;
        const QByteArray payload(256 * 1024, '#');
        for (int i = 0; i < 200; ++i) {
            const QString path = srcDir.filePath(QStringLiteral("ctrl_%1.bin").arg(i));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(payload), payload.size());
            f.close();
            sources.push_back(path);
        }

        QSignalSpy finishedSpy(&service, SIGNAL(Finished(QString)));
        QSignalSpy errorDetailSpy(&service, SIGNAL(ErrorDetail(QString,QString,QString)));
        QVERIFY(finishedSpy.isValid());
        QVERIFY(errorDetailSpy.isValid());

        auto waitForProcessExit = [](QProcess &proc, int timeoutMs) {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < timeoutMs) {
                if (proc.state() == QProcess::NotRunning) {
                    return true;
                }
                QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
                QThread::msleep(5);
            }
            return proc.state() == QProcess::NotRunning;
        };

        QProcess startProc;
        QStringList startArgs{QStringLiteral("copy"), dstDir.path()};
        startArgs.append(sources);
        startProc.setProgram(toolPath);
        startProc.setArguments(startArgs);
        startProc.start();
        QVERIFY(startProc.waitForStarted(2000));
        QVERIFY(waitForProcessExit(startProc, 20000));
        QCOMPARE(startProc.exitCode(), 0);
        const QString startOut = QString::fromUtf8(startProc.readAllStandardOutput()).trimmed();
        QVERIFY(!startOut.isEmpty());
        const QStringList lines = startOut.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY(!lines.isEmpty());
        const QString jobId = lines.constLast().trimmed();
        QVERIFY(!jobId.isEmpty());

        struct CtlResult {
            int exitCode = -1;
            QString out;
            QString err;
            bool started = false;
            bool finished = false;
        };

        auto runCtl = [&](const QString &cmd, const QString &id) -> CtlResult {
            CtlResult r;
            QProcess p;
            p.setProgram(toolPath);
            p.setArguments({cmd, id});
            p.start();
            if (!p.waitForStarted(2000)) {
                return r;
            }
            r.started = true;
            if (!waitForProcessExit(p, 10000)) {
                p.kill();
                waitForProcessExit(p, 1000);
                return r;
            }
            r.finished = true;
            r.exitCode = p.exitCode();
            r.out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            r.err = QString::fromUtf8(p.readAllStandardError());
            return r;
        };

        bool paused = false;
        QElapsedTimer pauseTimer;
        pauseTimer.start();
        while (pauseTimer.elapsed() < 6000) {
            const CtlResult res = runCtl(QStringLiteral("pause"), jobId);
            if (res.started && res.finished && res.exitCode == 0 && res.out == QStringLiteral("ok")) {
                paused = true;
                break;
            }
            QTest::qWait(20);
        }
        QVERIFY(paused);

        const CtlResult resumeRes = runCtl(QStringLiteral("resume"), jobId);
        QVERIFY(resumeRes.started && resumeRes.finished);
        QCOMPARE(resumeRes.exitCode, 0);
        QCOMPARE(resumeRes.out, QStringLiteral("ok"));

        const CtlResult pauseRes = runCtl(QStringLiteral("pause"), jobId);
        QVERIFY(pauseRes.started && pauseRes.finished);
        QCOMPARE(pauseRes.exitCode, 0);
        QCOMPARE(pauseRes.out, QStringLiteral("ok"));

        const CtlResult cancelRes = runCtl(QStringLiteral("cancel"), jobId);
        QVERIFY(cancelRes.started && cancelRes.finished);
        QCOMPARE(cancelRes.exitCode, 0);
        QCOMPARE(cancelRes.out, QStringLiteral("ok"));

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 10000);

        bool sawFinished = false;
        for (const QList<QVariant> &args : finishedSpy) {
            if (args.size() >= 1 && args.at(0).toString() == jobId) {
                sawFinished = true;
                break;
            }
        }
        QVERIFY(sawFinished);

        bool sawCancelledDetail = false;
        for (const QList<QVariant> &args : errorDetailSpy) {
            if (args.size() < 3) {
                continue;
            }
            if (args.at(0).toString() != jobId) {
                continue;
            }
            if (args.at(1).toString() == QString::fromLatin1(SlmFileOperationErrors::kCancelled)) {
                sawCancelledDetail = true;
                break;
            }
        }
        QVERIFY(sawCancelledDetail);

        // Unknown id should return failure status.
        const CtlResult unknownRes = runCtl(QStringLiteral("pause"),
                                            QStringLiteral("non-existent-id"));
        QVERIFY(unknownRes.started && unknownRes.finished);
        QCOMPARE(unknownRes.exitCode, 4);
        QCOMPARE(unknownRes.out, QStringLiteral("failed"));
    }

    void copyValidDestination_waitCompletesSuccessfully()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FileOperationsManager manager;
        FileOperationsService service(&manager);
        QVERIFY(service.serviceRegistered());

        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("fileopctl"));
        QVERIFY2(QFileInfo::exists(toolPath), qPrintable(QStringLiteral("missing tool: %1").arg(toolPath)));

        QTemporaryDir srcDir;
        QTemporaryDir dstDir;
        QVERIFY(srcDir.isValid());
        QVERIFY(dstDir.isValid());

        const QString srcFilePath = srcDir.filePath(QStringLiteral("sample.txt"));
        {
            QFile f(srcFilePath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QTextStream ts(&f);
            ts << "SLM Desktop fileopctl smoke test\n";
        }

        QProcess proc;
        proc.setProgram(toolPath);
        proc.setArguments({
            QStringLiteral("copy"),
            dstDir.path(),
            srcFilePath,
            QStringLiteral("--wait"),
        });
        proc.start();
        QVERIFY(proc.waitForStarted(2000));
        QElapsedTimer waitTimer;
        waitTimer.start();
        while (proc.state() != QProcess::NotRunning && waitTimer.elapsed() < 20000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
            QThread::msleep(5);
        }
        QVERIFY(proc.state() == QProcess::NotRunning);

        const int exitCode = proc.exitCode();
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        const QString err = QString::fromUtf8(proc.readAllStandardError());

        QCOMPARE(exitCode, 0);
        QVERIFY2(err.trimmed().isEmpty(), qPrintable(QStringLiteral("unexpected stderr: %1").arg(err)));

        QRegularExpression idRe(QStringLiteral("^[0-9a-fA-F\\-]{8,}$"));
        QString jobId;
        const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (idRe.match(trimmed).hasMatch()) {
                jobId = trimmed;
                break;
            }
        }
        QVERIFY2(!jobId.isEmpty(), qPrintable(QStringLiteral("job id not found in stdout: %1").arg(out)));
        QVERIFY2(out.contains(QStringLiteral("finished ") + jobId),
                 qPrintable(QStringLiteral("finished line not found for job id in stdout: %1").arg(out)));

        const QString copiedPath = dstDir.filePath(QStringLiteral("sample.txt"));
        QVERIFY2(QFileInfo::exists(copiedPath),
                 qPrintable(QStringLiteral("copied file missing: %1").arg(copiedPath)));
    }

    void copyInvalidDestination_reportsDetailedError()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FileOperationsManager manager;
        FileOperationsService service(&manager);
        QVERIFY(service.serviceRegistered());

        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("fileopctl"));
        QVERIFY2(QFileInfo::exists(toolPath), qPrintable(QStringLiteral("missing tool: %1").arg(toolPath)));

        const QString badDestination = QStringLiteral("/__slm_desktop_should_not_exist__/dest");

        QProcess proc;
        proc.setProgram(toolPath);
        proc.setArguments({
            QStringLiteral("copy"),
            badDestination,
            QStringLiteral("/tmp/irrelevant"),
            QStringLiteral("--wait"),
        });
        proc.start();
        QVERIFY(proc.waitForStarted(2000));

        QElapsedTimer timer;
        timer.start();
        while (proc.state() != QProcess::NotRunning && timer.elapsed() < 20000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }
        if (proc.state() != QProcess::NotRunning) {
            proc.kill();
            for (int i = 0; i < 20 && proc.state() != QProcess::NotRunning; ++i) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                QThread::msleep(5);
            }
            QFAIL("fileopctl did not finish in time");
        }

        const int exitCode = proc.exitCode();
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        const QString err = QString::fromUtf8(proc.readAllStandardError());

        QCOMPARE(exitCode, 4);
        QVERIFY2(out.contains(QRegularExpression(QStringLiteral(
                             "^[0-9a-fA-F\\-]{8,}\\s*$"),
                             QRegularExpression::MultilineOption)),
                 qPrintable(QStringLiteral("unexpected stdout: %1").arg(out)));
        QVERIFY2(err.contains(QStringLiteral("code=")
                              + QString::fromLatin1(SlmFileOperationErrors::kInvalidDestination)),
                 qPrintable(QStringLiteral("missing error code in stderr: %1").arg(err)));
        QVERIFY2(err.contains(QStringLiteral("message=") + badDestination),
                 qPrintable(QStringLiteral("missing error message in stderr: %1").arg(err)));
    }
};

QTEST_MAIN(FileopctlSmokeTest)
#include "fileopctl_smoke_test.moc"
