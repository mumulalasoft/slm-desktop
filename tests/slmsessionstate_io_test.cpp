#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTest>

#include "src/login/libslmlogin/slmsessionstate.h"

using namespace Slm::Login;

class SlmSessionStateIoTest : public QObject
{
    Q_OBJECT

private slots:
    void saveLoadRoundTrip();
};

void SlmSessionStateIoTest::saveLoadRoundTrip()
{
    SessionState input = SessionState::defaults();
    input.crashCount = 2;
    input.lastMode = StartupMode::Safe;
    input.lastGoodSnapshot = QStringLiteral("20260327-001");
    input.safeModeForced = true;
    input.configPending = true;
    input.recoveryReason = QStringLiteral("test");
    input.lastBootStatus = QStringLiteral("started");

    QString err;
    QVERIFY(SessionStateIO::save(input, err));

    SessionState output;
    QVERIFY(SessionStateIO::load(output, err));
    QCOMPARE(output.crashCount, 2);
    QCOMPARE(output.lastMode, StartupMode::Safe);
    QCOMPARE(output.lastGoodSnapshot, QStringLiteral("20260327-001"));
    QCOMPARE(output.safeModeForced, true);
    QCOMPARE(output.configPending, true);
    QCOMPARE(output.recoveryReason, QStringLiteral("test"));
    QCOMPARE(output.lastBootStatus, QStringLiteral("started"));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString cfgRoot = QStringLiteral("/tmp/slm-login-tests-sessionstate-%1")
                                .arg(QCoreApplication::applicationPid());
    QDir().mkpath(cfgRoot);
    qputenv("XDG_CONFIG_HOME", cfgRoot.toLocal8Bit());
    QStandardPaths::setTestModeEnabled(false);
    SlmSessionStateIoTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "slmsessionstate_io_test.moc"
