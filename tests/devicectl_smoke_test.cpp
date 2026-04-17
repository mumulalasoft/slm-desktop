#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include "../src/daemon/devicesd/devicesmanager.h"
#include "../src/daemon/devicesd/devicesservice.h"

class DevicectlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void cliContract_exitCodesAndJsonPayload()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        DevicesManager manager;
        DevicesService service(&manager);
        QVERIFY(service.serviceRegistered());

        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("devicectl"));
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

        auto assertInvalidTargetResult = [&](const CtlResult &r) {
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 4);
            const QJsonDocument doc = QJsonDocument::fromJson(r.out);
            QVERIFY2(doc.isObject(), qPrintable(QStringLiteral("invalid json output: %1")
                                                       .arg(QString::fromUtf8(r.out))));
            const QJsonObject obj = doc.object();
            QVERIFY(!obj.value(QStringLiteral("ok")).toBool());
            QCOMPARE(obj.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-target"));
        };

        assertInvalidTargetResult(runCtl({QStringLiteral("mount"), QStringLiteral(""), QStringLiteral("--pretty")}));
        assertInvalidTargetResult(runCtl({QStringLiteral("eject"), QStringLiteral(""), QStringLiteral("--pretty")}));
        assertInvalidTargetResult(runCtl({QStringLiteral("unlock"), QStringLiteral(""), QStringLiteral("pw"), QStringLiteral("--pretty")}));
        assertInvalidTargetResult(runCtl({QStringLiteral("format"), QStringLiteral(""), QStringLiteral("ext4"), QStringLiteral("--pretty")}));
    }
};

QTEST_MAIN(DevicectlSmokeTest)
#include "devicectl_smoke_test.moc"

