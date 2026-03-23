#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

class GlobalmenuctlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void cliContract_helpDumpHealthcheck()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QString toolPath =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("globalmenuctl"));
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
            const CtlResult r = runCtl({QStringLiteral("--help")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 0);
            QVERIFY(QString::fromUtf8(r.out).contains(QStringLiteral("globalmenuctl healthcheck")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("dump")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 0);
            const QJsonDocument doc = QJsonDocument::fromJson(r.out);
            QVERIFY2(doc.isObject(),
                     qPrintable(QStringLiteral("invalid json dump: %1").arg(QString::fromUtf8(r.out))));
            const QJsonObject root = doc.object();
            QVERIFY(root.contains(QStringLiteral("registrars")));
            QVERIFY(root.contains(QStringLiteral("activeMenuBinding")));
            QVERIFY(root.contains(QStringLiteral("topLevelMenus")));
            QVERIFY(root.contains(QStringLiteral("slmFallback")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("healthcheck")});
            QVERIFY(r.started && r.finished);
            QVERIFY(r.exitCode == 0 || r.exitCode == 2);
            const QJsonDocument doc = QJsonDocument::fromJson(r.out);
            QVERIFY2(doc.isObject(),
                     qPrintable(QStringLiteral("invalid json healthcheck: %1").arg(QString::fromUtf8(r.out))));
            const QJsonObject root = doc.object();
            QVERIFY(root.contains(QStringLiteral("ok")));
            QVERIFY(root.value(QStringLiteral("ok")).isBool());
            QVERIFY(root.contains(QStringLiteral("activeMenuBinding")));
        }
    }
};

QTEST_MAIN(GlobalmenuctlSmokeTest)
#include "globalmenuctl_smoke_test.moc"
