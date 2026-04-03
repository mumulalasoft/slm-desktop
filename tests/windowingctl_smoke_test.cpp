#include <QtTest/QtTest>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

class WindowingctlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void cliMatrixAndWatch_emitValidJson()
    {
        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("windowingctl"));
        QVERIFY2(QFileInfo::exists(toolPath), qPrintable(QStringLiteral("missing tool: %1").arg(toolPath)));

        struct CtlResult {
            bool started = false;
            bool finished = false;
            int exitCode = -1;
            QByteArray out;
            QByteArray err;
        };

        auto runCtl = [&](const QStringList &args, int timeoutMs = 5000) -> CtlResult {
            CtlResult r;
            QProcess p;
            p.setProgram(toolPath);
            p.setArguments(args);
            p.start();
            if (!p.waitForStarted(2000)) {
                return r;
            }
            r.started = true;
            if (!p.waitForFinished(timeoutMs)) {
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
            const CtlResult r = runCtl({QStringLiteral("matrix"), QStringLiteral("--pretty")});
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 0);
            const QJsonDocument doc = QJsonDocument::fromJson(r.out);
            QVERIFY2(doc.isObject(), qPrintable(QStringLiteral("invalid matrix json: %1")
                                                       .arg(QString::fromUtf8(r.out))));
            const QJsonObject root = doc.object();
            QVERIFY(root.contains(QStringLiteral("activeBackend")));
            QVERIFY(root.contains(QStringLiteral("runtimeCapabilities")));
            QVERIFY(root.contains(QStringLiteral("staticMatrix")));
            const QJsonObject staticMatrix = root.value(QStringLiteral("staticMatrix")).toObject();
            QVERIFY(staticMatrix.contains(QStringLiteral("kwin-wayland")));
        }

        {
            const CtlResult r = runCtl({QStringLiteral("watch"),
                                        QStringLiteral("--duration-ms"),
                                        QStringLiteral("10")},
                                       3000);
            QVERIFY(r.started && r.finished);
            QCOMPARE(r.exitCode, 0);
            const QStringList lines =
                    QString::fromUtf8(r.out).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            QVERIFY(!lines.isEmpty());
            const QJsonDocument first = QJsonDocument::fromJson(lines.first().toUtf8());
            QVERIFY2(first.isObject(), qPrintable(QStringLiteral("invalid watch json: %1")
                                                         .arg(lines.first())));
            const QJsonObject obj = first.object();
            QCOMPARE(obj.value(QStringLiteral("kind")).toString(), QStringLiteral("snapshot"));
            QVERIFY(obj.contains(QStringLiteral("activeBackend")));
        }
    }

    void cliInvalidCommand_returnsUsageError()
    {
        const QString toolPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("windowingctl"));
        QVERIFY2(QFileInfo::exists(toolPath), qPrintable(QStringLiteral("missing tool: %1").arg(toolPath)));

        QProcess p;
        p.setProgram(toolPath);
        p.setArguments({QStringLiteral("unknown-cmd")});
        p.start();
        QVERIFY(p.waitForStarted(2000));
        QVERIFY(p.waitForFinished(5000));
        QCOMPARE(p.exitCode(), 1);
        const QString err = QString::fromUtf8(p.readAllStandardError());
        QVERIFY(err.contains(QStringLiteral("unknown command")));
    }
};

QTEST_MAIN(WindowingctlSmokeTest)
#include "windowingctl_smoke_test.moc"

