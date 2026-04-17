#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>

#include "../src/daemon/desktopd/desktopdaemonservice.h"
#include "../src/core/workspace/spacesmanager.h"
#include "../src/core/workspace/windowingbackendmanager.h"
#include "../src/core/workspace/workspacemanager.h"

class WorkspacectlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void cliCommands_workAgainstWorkspaceService()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend, &spaces, backend.compositorStateObject());
        DesktopDaemonService service(&workspace, &backend, &spaces);
        QVERIFY(service.serviceRegistered());

        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("workspacectl"));
        QVERIFY2(QFileInfo::exists(toolPath), qPrintable(QStringLiteral("missing tool: %1").arg(toolPath)));

        struct CtlResult {
            bool started = false;
            bool finished = false;
            int exitCode = -1;
            QByteArray out;
            QByteArray err;
        };

        auto runCtl = [&](const QStringList &args) -> CtlResult {
            CtlResult r;
            QProcess p;
            p.setProgram(toolPath);
            p.setArguments(args);
            p.start();
            if (!p.waitForStarted(2000)) {
                return r;
            }
            r.started = true;
            if (!p.waitForFinished(5000)) {
                p.kill();
                p.waitForFinished(1000);
                return r;
            }
            r.finished = true;
            r.exitCode = p.exitCode();
            r.out = p.readAllStandardOutput();
            r.err = p.readAllStandardError();
            return r;
        };

        {
            const CtlResult p = runCtl({QStringLiteral("healthcheck")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 0);
            const QByteArray out = p.out;
            const QJsonDocument doc = QJsonDocument::fromJson(out);
            QVERIFY2(doc.isObject(), qPrintable(QStringLiteral("invalid json: %1").arg(QString::fromUtf8(out))));
            const QJsonObject root = doc.object();
            QVERIFY(root.value(QStringLiteral("ok")).toBool());
            const QJsonObject primary = root.value(QStringLiteral("primary")).toObject();
            QVERIFY(primary.value(QStringLiteral("registered")).toBool());
        }

        {
            const CtlResult p = runCtl({QStringLiteral("overview"), QStringLiteral("toggle")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 0);
        }

        {
            const CtlResult p = runCtl({QStringLiteral("switch"), QStringLiteral("2")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 0);
            QCOMPARE(spaces.activeSpace(), 2);
        }

        {
            const CtlResult p = runCtl({QStringLiteral("switch-delta"), QStringLiteral("-1")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 0);
            QCOMPARE(spaces.activeSpace(), 1);
        }

        {
            const CtlResult p = runCtl({QStringLiteral("move"), QStringLiteral("kwin:smoke-view"), QStringLiteral("3")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 0);
            QCOMPARE(QString::fromUtf8(p.out).trimmed(), QStringLiteral("ok"));
            QCOMPARE(spaces.windowSpace(QStringLiteral("kwin:smoke-view")), 3);
        }

        {
            const CtlResult p = runCtl({QStringLiteral("move-focused-delta"), QStringLiteral("1")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 4);
            QCOMPARE(QString::fromUtf8(p.out).trimmed(), QStringLiteral("failed"));
        }

        {
            const CtlResult p = runCtl({QStringLiteral("present"), QStringLiteral("not-found-app")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 4);
            QCOMPARE(QString::fromUtf8(p.out).trimmed(), QStringLiteral("not-found"));
        }

        {
            const CtlResult p = runCtl({QStringLiteral("ranked-apps"), QStringLiteral("5")});
            QVERIFY(p.started && p.finished);
            QCOMPARE(p.exitCode, 0);
            const QJsonDocument doc = QJsonDocument::fromJson(p.out);
            QVERIFY2(doc.isArray(),
                     qPrintable(QStringLiteral("invalid ranked-apps json: %1")
                                    .arg(QString::fromUtf8(p.out))));
            const QJsonArray arr = doc.array();
            QVERIFY(arr.size() <= 5);
            for (int i = 1; i < arr.size(); ++i) {
                const QJsonObject prev = arr.at(i - 1).toObject();
                const QJsonObject curr = arr.at(i).toObject();
                QVERIFY(prev.value(QStringLiteral("score")).toInt()
                        >= curr.value(QStringLiteral("score")).toInt());
            }
        }
    }

    void cliInvalidArgs_returnExpectedExitCodes()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend, &spaces, backend.compositorStateObject());
        DesktopDaemonService service(&workspace, &backend, &spaces);
        QVERIFY(service.serviceRegistered());

        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("workspacectl"));
        QVERIFY2(QFileInfo::exists(toolPath), qPrintable(QStringLiteral("missing tool: %1").arg(toolPath)));

        struct CtlResult {
            bool started = false;
            bool finished = false;
            int exitCode = -1;
            QByteArray out;
            QByteArray err;
        };

        auto runCtl = [&](const QStringList &args) -> CtlResult {
            CtlResult r;
            QProcess p;
            p.setProgram(toolPath);
            p.setArguments(args);
            p.start();
            if (!p.waitForStarted(2000)) {
                return r;
            }
            r.started = true;
            if (!p.waitForFinished(5000)) {
                p.kill();
                p.waitForFinished(1000);
                return r;
            }
            r.finished = true;
            r.exitCode = p.exitCode();
            r.out = p.readAllStandardOutput();
            r.err = p.readAllStandardError();
            return r;
        };

        {
            const CtlResult r = runCtl({});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.out).contains(QStringLiteral("Usage:")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("unknown-cmd")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.err).contains(QStringLiteral("unknown command")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("switch"), QStringLiteral("abc")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.err).contains(QStringLiteral("invalid index")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("move"), QStringLiteral("kwin:view"), QStringLiteral("abc")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.err).contains(QStringLiteral("invalid index")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("switch-delta"), QStringLiteral("abc")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.err).contains(QStringLiteral("invalid delta")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("move-focused-delta"), QStringLiteral("abc")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.err).contains(QStringLiteral("invalid delta")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("overview"), QStringLiteral("invalid-mode")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.err).contains(QStringLiteral("invalid overview mode")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("ranked-apps"), QStringLiteral("abc")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 1);
            QVERIFY(QString::fromUtf8(r.err).contains(QStringLiteral("invalid limit")));
        }
    }
};

QTEST_MAIN(WorkspacectlSmokeTest)
#include "workspacectl_smoke_test.moc"
