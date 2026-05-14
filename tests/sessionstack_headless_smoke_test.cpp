#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"

using namespace Slm::Login;

namespace {

QJsonObject makeConfig(const QString &compositor, const QString &shell)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("compositor"), compositor);
    obj.insert(QStringLiteral("compositorArgs"), QJsonArray{});
    obj.insert(QStringLiteral("shell"), shell);
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

class SessionStackHeadlessSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void brokerAndWatchdogReachHealthyState();

private:
    void resetEnvironment();
    QString runtimeBinaryPath(const QString &name) const;
    QString tempBinaryPath(const QString &name) const;
    void installServiceStubs();
    void installPgrepStub();
    void installFakeShell();
    QString installFakeCompositor();
    void writeActiveConfig(const QString &compositorPath, const QString &shellPath);

    QTemporaryDir m_tempDir;
    QString m_binDir;
    QString m_cfgRoot;
    QString m_runtimeDir;
};

void SessionStackHeadlessSmokeTest::init()
{
    QVERIFY2(m_tempDir.isValid(), "temporary test root is invalid");
    m_binDir = m_tempDir.path() + QStringLiteral("/bin");
    m_cfgRoot = m_tempDir.path() + QStringLiteral("/config");
    m_runtimeDir = m_tempDir.path() + QStringLiteral("/runtime");
    resetEnvironment();
}

void SessionStackHeadlessSmokeTest::brokerAndWatchdogReachHealthyState()
{
    installServiceStubs();
    installPgrepStub();
    installFakeShell();
    const QString compositorPath = installFakeCompositor();
    const QString shellPath = tempBinaryPath(QStringLiteral("slm-shell"));
    writeActiveConfig(compositorPath, shellPath);

    QProcess broker;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PATH"),
               m_binDir + QStringLiteral(":")
               + QCoreApplication::applicationDirPath()
               + QStringLiteral(":/usr/bin:/bin"));
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), m_cfgRoot);
    env.insert(QStringLiteral("XDG_RUNTIME_DIR"), m_runtimeDir);
    env.insert(QStringLiteral("SLM_WATCHDOG_HEALTHY_TIMEOUT_MS"), QStringLiteral("50"));
    env.insert(QStringLiteral("SLM_TEST_ACCEPT_REGULAR_WAYLAND_SOCKET"), QStringLiteral("1"));
    broker.setProcessEnvironment(env);
    broker.setProgram(runtimeBinaryPath(QStringLiteral("slm-session-broker")));
    broker.setArguments({QStringLiteral("--mode"), QStringLiteral("normal")});
    broker.start();
    QVERIFY2(broker.waitForStarted(5000), qPrintable(broker.errorString()));

    QVERIFY2(broker.waitForFinished(5000), "broker did not finish");
    QCOMPARE(broker.exitStatus(), QProcess::NormalExit);
    QCOMPARE(broker.exitCode(), 0);

    const SessionState state = loadStateOrFail();
    QCOMPARE(state.lastMode, StartupMode::Normal);
    QCOMPARE(state.crashCount, 0);
    QCOMPARE(state.lastBootStatus, QStringLiteral("healthy"));
    QCOMPARE(state.configPending, false);
    QVERIFY(state.recoveryReason.isEmpty());
    QVERIFY2(!state.lastGoodSnapshot.isEmpty(), "last_good_snapshot should be populated");
    QVERIFY(QFileInfo::exists(ConfigManager::snapshotPath(state.lastGoodSnapshot)));
    QVERIFY(QFileInfo::exists(ConfigManager::safePath()));
    QVERIFY(QFileInfo::exists(m_tempDir.path() + QStringLiteral("/shell.started")));
    QVERIFY(QFileInfo::exists(m_tempDir.path() + QStringLiteral("/watchdog.promoted")));
}

void SessionStackHeadlessSmokeTest::resetEnvironment()
{
    QDir(m_tempDir.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_binDir));
    QVERIFY(QDir().mkpath(m_cfgRoot));
    QVERIFY(QDir().mkpath(m_runtimeDir));

    qputenv("XDG_CONFIG_HOME", m_cfgRoot.toLocal8Bit());
    qputenv("XDG_RUNTIME_DIR", m_runtimeDir.toLocal8Bit());
    QStandardPaths::setTestModeEnabled(false);
}

QString SessionStackHeadlessSmokeTest::runtimeBinaryPath(const QString &name) const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/") + name;
}

QString SessionStackHeadlessSmokeTest::tempBinaryPath(const QString &name) const
{
    return m_binDir + QStringLiteral("/") + name;
}

void SessionStackHeadlessSmokeTest::installServiceStubs()
{
    const QStringList names = {
        QStringLiteral("desktopd"),
        QStringLiteral("slm-svcmgrd"),
        QStringLiteral("slm-loggerd"),
    };
    for (const QString &name : names) {
        requireExecutable(tempBinaryPath(name),
                          QStringLiteral("#!/bin/sh\nexit 0\n").toUtf8());
    }
}

void SessionStackHeadlessSmokeTest::installPgrepStub()
{
    requireExecutable(tempBinaryPath(QStringLiteral("pgrep")),
                      QStringLiteral(
                          "#!/bin/sh\n"
                          "if [ \"$1\" = \"-x\" ] && [ \"$2\" = \"slm-desktop\" ]; then\n"
                          "  exit 0\n"
                          "fi\n"
                          "if [ \"$1\" = \"-x\" ] && [ \"$2\" = \"slm-shell\" ]; then\n"
                          "  exit 0\n"
                          "fi\n"
                          "exit 1\n")
                          .toUtf8());
}

void SessionStackHeadlessSmokeTest::installFakeShell()
{
    requireExecutable(tempBinaryPath(QStringLiteral("slm-shell")),
                      QStringLiteral(
                          "#!/bin/sh\n"
                          "touch \"$XDG_CONFIG_HOME/../shell.started\"\n"
                          "sleep 0.2\n"
                          "exit 0\n")
                          .toUtf8());
}

QString SessionStackHeadlessSmokeTest::installFakeCompositor()
{
    const QString path = tempBinaryPath(QStringLiteral("kwin_wayland"));
    requireExecutable(path,
                          QStringLiteral(
                          "#!/bin/sh\n"
                          "if [ \"$1\" = \"--help\" ]; then exit 0; fi\n"
                          "socket_name=wayland-0\n"
                          "while [ \"$#\" -gt 0 ]; do\n"
                          "  case \"$1\" in\n"
                          "    --socket|--wayland-display)\n"
                          "      shift\n"
                          "      [ \"$#\" -gt 0 ] && socket_name=\"$1\"\n"
                          "      ;;\n"
                          "  esac\n"
                          "  shift\n"
                          "done\n"
                          "touch \"$XDG_RUNTIME_DIR/$socket_name\"\n"
                          "state_file=\"$XDG_CONFIG_HOME/slm-desktop/state.json\"\n"
                          "i=0\n"
                          "while [ \"$i\" -lt 200 ]; do\n"
                          "  if [ -f \"$state_file\" ] && grep -q '\"last_boot_status\": \"healthy\"' \"$state_file\"; then\n"
                          "    touch \"$XDG_CONFIG_HOME/../watchdog.promoted\"\n"
                          "    exit 0\n"
                          "  fi\n"
                          "  sleep 0.05\n"
                          "  i=$((i + 1))\n"
                          "done\n"
                          "echo 'timeout waiting for healthy state' >&2\n"
                          "exit 2\n")
                          .toUtf8());
    return path;
}

void SessionStackHeadlessSmokeTest::writeActiveConfig(const QString &compositorPath,
                                                      const QString &shellPath)
{
    ConfigManager config;
    QString err;
    QVERIFY2(config.save(makeConfig(compositorPath, shellPath), &err), qPrintable(err));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    SessionStackHeadlessSmokeTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "sessionstack_headless_smoke_test.moc"
