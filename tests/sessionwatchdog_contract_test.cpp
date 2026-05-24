#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"
#include "src/login/watchdog/sessionwatchdog.h"

using namespace Slm::Login;

namespace {

QJsonObject makeConfig()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("compositor"), QStringLiteral("kwin_wayland"));
    obj.insert(QStringLiteral("compositorArgs"), QJsonArray{});
    obj.insert(QStringLiteral("shell"), QStringLiteral("slm-shell"));
    obj.insert(QStringLiteral("shellArgs"), QJsonArray{});
    return obj;
}

bool writeExecutable(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (file.write(content) != content.size()) {
        file.close();
        return false;
    }
    file.close();
    return QFile::setPermissions(path, QFileDevice::ReadOwner
                                       | QFileDevice::WriteOwner
                                       | QFileDevice::ExeOwner
                                       | QFileDevice::ReadUser
                                       | QFileDevice::ExeUser
                                       | QFileDevice::ReadGroup
                                       | QFileDevice::ExeGroup
                                       | QFileDevice::ReadOther
                                       | QFileDevice::ExeOther);
}

void requireExecutable(const QString &path, const QByteArray &content)
{
    if (!writeExecutable(path, content)) {
        qFatal("failed to create executable: %s", qUtf8Printable(path));
    }
}

SessionState loadStateOrFail()
{
    SessionState state;
    QString err;
    if (!SessionStateIO::load(state, err)) {
        qFatal("failed to load session state: %s", qUtf8Printable(err));
    }
    return state;
}

} // namespace

class SessionWatchdogContractTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void marksSessionHealthyAndClearsPending();
    void skipsHealthyMarkWhenShellIsMissing();

private:
    void resetEnvironment();
    void seedConfig();
    void seedState();
    void installPgrepStub(int exitCode);

    QTemporaryDir m_tempDir;
    QString m_binDir;
    QString m_cfgRoot;
    QString m_runtimeDir;
    QByteArray m_originalConfigHome;
    QByteArray m_originalRuntimeDir;
};

void SessionWatchdogContractTest::init()
{
    QVERIFY2(m_tempDir.isValid(), "temporary test root is invalid");
    m_binDir = m_tempDir.path() + QStringLiteral("/bin");
    m_cfgRoot = m_tempDir.path() + QStringLiteral("/config");
    m_runtimeDir = m_tempDir.path() + QStringLiteral("/runtime");

    if (m_originalConfigHome.isEmpty()) {
        m_originalConfigHome = qgetenv("XDG_CONFIG_HOME");
        m_originalRuntimeDir = qgetenv("XDG_RUNTIME_DIR");
    }

    resetEnvironment();
}

void SessionWatchdogContractTest::marksSessionHealthyAndClearsPending()
{
    seedConfig();
    seedState();
    installPgrepStub(0);

    QFile waylandSocket(m_runtimeDir + QStringLiteral("/wayland-0"));
    QVERIFY(waylandSocket.open(QIODevice::WriteOnly));
    waylandSocket.close();

    SessionWatchdog watchdog;
    QTRY_VERIFY_WITH_TIMEOUT(loadStateOrFail().lastBootStatus == QStringLiteral("healthy"), 1000);

    const SessionState state = loadStateOrFail();
    QCOMPARE(state.crashCount, 0);
    QCOMPARE(state.lastBootStatus, QStringLiteral("healthy"));
    QCOMPARE(state.configPending, false);
    QCOMPARE(state.safeModeForced, false);
    QVERIFY(state.recoveryReason.isEmpty());
    QVERIFY(state.lastCrashReason.isEmpty());
    QVERIFY2(!state.lastGoodSnapshot.isEmpty(), "last_good_snapshot should be populated");
    QVERIFY(QFileInfo::exists(ConfigManager::snapshotPath(state.lastGoodSnapshot)));
    QVERIFY(QFileInfo::exists(ConfigManager::safePath()));
}

void SessionWatchdogContractTest::skipsHealthyMarkWhenShellIsMissing()
{
    seedConfig();
    seedState();
    installPgrepStub(1);

    QFile waylandSocket(m_runtimeDir + QStringLiteral("/wayland-0"));
    QVERIFY(waylandSocket.open(QIODevice::WriteOnly));
    waylandSocket.close();

    SessionWatchdog watchdog;
    QTest::qWait(100);

    const SessionState state = loadStateOrFail();
    QCOMPARE(state.crashCount, 2);
    QCOMPARE(state.lastBootStatus, QStringLiteral("started"));
    QCOMPARE(state.configPending, true);
    QCOMPARE(state.lastCrashReason, QStringLiteral("previous-crash"));
    QVERIFY(state.lastGoodSnapshot.isEmpty());
    QVERIFY(!QFileInfo::exists(ConfigManager::safePath()));
}

void SessionWatchdogContractTest::resetEnvironment()
{
    QDir(m_tempDir.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_binDir));
    QVERIFY(QDir().mkpath(m_cfgRoot));
    QVERIFY(QDir().mkpath(m_runtimeDir));

    qputenv("PATH", (m_binDir.toLocal8Bit() + QByteArray(":/usr/bin:/bin")));
    qputenv("XDG_CONFIG_HOME", m_cfgRoot.toLocal8Bit());
    qputenv("XDG_RUNTIME_DIR", m_runtimeDir.toLocal8Bit());
    qputenv("WAYLAND_DISPLAY", QByteArray("wayland-0"));
    qputenv("SLM_WATCHDOG_HEALTHY_TIMEOUT_MS", QByteArray("10"));

    QStandardPaths::setTestModeEnabled(false);
}

void SessionWatchdogContractTest::seedConfig()
{
    ConfigManager config;
    QString err;
    QVERIFY2(config.save(makeConfig(), &err), qPrintable(err));
}

void SessionWatchdogContractTest::seedState()
{
    SessionState state = SessionState::defaults();
    state.crashCount = 2;
    state.lastMode = StartupMode::Normal;
    state.configPending = true;
    state.safeModeForced = true;
    state.recoveryReason = QStringLiteral("previous-failure");
    state.lastCrashReason = QStringLiteral("previous-crash");
    state.lastBootStatus = QStringLiteral("started");

    QString err;
    QVERIFY2(SessionStateIO::save(state, err), qPrintable(err));
}

void SessionWatchdogContractTest::installPgrepStub(int exitCode)
{
    const QString script = QStringLiteral(
        "#!/bin/sh\n"
        "if [ \"$1\" = \"-x\" ] && [ \"$2\" = \"slm-desktop\" ]; then\n"
        "  exit %1\n"
        "fi\n"
        "if [ \"$1\" = \"-x\" ] && [ \"$2\" = \"slm-shell\" ]; then\n"
        "  exit %1\n"
        "fi\n"
        "exit %1\n")
        .arg(exitCode);
    requireExecutable(m_binDir + QStringLiteral("/pgrep"), script.toUtf8());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    SessionWatchdogContractTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "sessionwatchdog_contract_test.moc"
