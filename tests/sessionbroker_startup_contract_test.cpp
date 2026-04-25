#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"
#include "src/login/session-broker/sessionbroker.h"

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

QByteArray stubScript(const QString &name)
{
    return QStringLiteral(
               "#!/bin/sh\n"
               "printf '%1\\n' \"$0\" > \"$SLM_TEST_ROOT/%1.invoked\"\n"
               "exit 0\n")
        .arg(name)
        .toUtf8();
}

SessionState loadStateOrFail()
{
    SessionState state;
    QString err;
    const bool ok = SessionStateIO::load(state, err);
    if (!ok) {
        qFatal("failed to load session state: %s", qUtf8Printable(err));
    }
    return state;
}

QString requireExecutable(const QString &path, const QByteArray &content)
{
    if (!writeExecutable(path, content)) {
        qFatal("failed to create executable: %s", qUtf8Printable(path));
    }
    return path;
}

} // namespace

class SessionBrokerStartupContractTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void normalStartupLaunchesCompositorShellAndWatchdog();
    void missingWatchdogFailsPreflight();

private:
    void resetEnvironment();
    QString createBinary(const QString &name, const QByteArray &content);
    void writeActiveConfig(const QString &compositor, const QString &shell);
    QString markerPath(const QString &name) const;

    QTemporaryDir m_tempDir;
    QString m_binDir;
    QString m_cfgRoot;
    QString m_runtimeDir;
    QByteArray m_originalConfigHome;
    QByteArray m_originalRuntimeDir;
};

void SessionBrokerStartupContractTest::init()
{
    QVERIFY2(m_tempDir.isValid(), "temporary test root is invalid");
    m_binDir = m_tempDir.path() + QStringLiteral("/bin");
    m_cfgRoot = m_tempDir.path() + QStringLiteral("/config");
    m_runtimeDir = m_tempDir.path() + QStringLiteral("/runtime");
    QVERIFY(QDir().mkpath(m_binDir));
    QVERIFY(QDir().mkpath(m_cfgRoot));
    QVERIFY(QDir().mkpath(m_runtimeDir));

    if (m_originalConfigHome.isEmpty()) {
        m_originalConfigHome = qgetenv("XDG_CONFIG_HOME");
        m_originalRuntimeDir = qgetenv("XDG_RUNTIME_DIR");
    }

    resetEnvironment();
}

void SessionBrokerStartupContractTest::normalStartupLaunchesCompositorShellAndWatchdog()
{
    const QString compositor = createBinary(
        QStringLiteral("kwin_wayland"),
        QStringLiteral(
            "#!/bin/sh\n"
            "printf '%s\\n' \"$SLM_SESSION_MODE\" > \"$SLM_TEST_ROOT/compositor.mode\"\n"
            "touch \"$XDG_RUNTIME_DIR/wayland-0\"\n"
            "i=0\n"
            "while [ \"$i\" -lt 100 ]; do\n"
            "  [ -f \"$SLM_TEST_ROOT/stop-compositor\" ] && exit 0\n"
            "  sleep 0.05\n"
            "  i=$((i + 1))\n"
            "done\n"
            "exit 0\n")
            .toUtf8());
    const QString shell = createBinary(
        QStringLiteral("slm-shell"),
        QStringLiteral(
            "#!/bin/sh\n"
            "printf '%s\\n' \"$SLM_SESSION_MODE\" > \"$SLM_TEST_ROOT/shell.mode\"\n"
            "touch \"$SLM_TEST_ROOT/stop-compositor\"\n"
            "exit 0\n")
            .toUtf8());
    createBinary(QStringLiteral("slm-watchdog"),
                 QStringLiteral(
                     "#!/bin/sh\n"
                     "printf '%s\\n' \"$SLM_SESSION_MODE\" > \"$SLM_TEST_ROOT/watchdog.mode\"\n"
                     "exit 0\n")
                     .toUtf8());
    createBinary(QStringLiteral("desktopd"), stubScript(QStringLiteral("desktopd")));
    createBinary(QStringLiteral("slm-svcmgrd"), stubScript(QStringLiteral("slm-svcmgrd")));
    createBinary(QStringLiteral("slm-loggerd"), stubScript(QStringLiteral("slm-loggerd")));

    writeActiveConfig(compositor, shell);

    SessionBroker broker(StartupMode::Normal);
    QCOMPARE(broker.run(), 0);

    QVERIFY2(QFileInfo::exists(markerPath(QStringLiteral("shell.mode"))),
             "shell was not launched");
    QVERIFY2(QFileInfo::exists(markerPath(QStringLiteral("watchdog.mode"))),
             "watchdog was not launched");
    QVERIFY2(QFileInfo::exists(markerPath(QStringLiteral("compositor.mode"))),
             "compositor did not run");

    QFile shellMode(markerPath(QStringLiteral("shell.mode")));
    QVERIFY(shellMode.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(shellMode.readAll()).trimmed(), QStringLiteral("normal"));

    QFile watchdogMode(markerPath(QStringLiteral("watchdog.mode")));
    QVERIFY(watchdogMode.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(watchdogMode.readAll()).trimmed(), QStringLiteral("normal"));

    const SessionState state = loadStateOrFail();
    QCOMPARE(state.lastMode, StartupMode::Normal);
    QCOMPARE(state.lastBootStatus, QStringLiteral("ended"));
    QCOMPARE(state.recoveryReason, QString());
    QCOMPARE(state.crashCount, 1);
    QVERIFY(state.configPending);
}

void SessionBrokerStartupContractTest::missingWatchdogFailsPreflight()
{
    const QString compositor = createBinary(
        QStringLiteral("kwin_wayland"),
        QStringLiteral(
            "#!/bin/sh\n"
            "touch \"$XDG_RUNTIME_DIR/wayland-0\"\n"
            "exit 0\n")
            .toUtf8());
    const QString shell = createBinary(
        QStringLiteral("slm-shell"),
        QStringLiteral(
            "#!/bin/sh\n"
            "touch \"$SLM_TEST_ROOT/unexpected-shell\"\n"
            "exit 0\n")
            .toUtf8());

    createBinary(QStringLiteral("desktopd"), stubScript(QStringLiteral("desktopd")));
    createBinary(QStringLiteral("slm-svcmgrd"), stubScript(QStringLiteral("slm-svcmgrd")));
    createBinary(QStringLiteral("slm-loggerd"), stubScript(QStringLiteral("slm-loggerd")));

    writeActiveConfig(compositor, shell);

    SessionBroker broker(StartupMode::Normal);
    QCOMPARE(broker.run(), 1);

    QVERIFY2(!QFileInfo::exists(markerPath(QStringLiteral("unexpected-shell"))),
             "shell should not launch when watchdog preflight fails");

    const SessionState state = loadStateOrFail();
    QCOMPARE(state.lastMode, StartupMode::Normal);
    QCOMPARE(state.lastBootStatus, QStringLiteral("crashed"));
    QVERIFY(state.recoveryReason.startsWith(QStringLiteral("missing-component:slm-watchdog")));
    QCOMPARE(state.crashCount, 1);
}

void SessionBrokerStartupContractTest::resetEnvironment()
{
    QDir(m_tempDir.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_binDir));
    QVERIFY(QDir().mkpath(m_cfgRoot));
    QVERIFY(QDir().mkpath(m_runtimeDir));

    qputenv("PATH", (m_binDir.toLocal8Bit() + QByteArray(":/usr/bin:/bin")));
    qputenv("XDG_CONFIG_HOME", m_cfgRoot.toLocal8Bit());
    qputenv("XDG_RUNTIME_DIR", m_runtimeDir.toLocal8Bit());
    qputenv("SLM_TEST_ROOT", m_tempDir.path().toLocal8Bit());

    QStandardPaths::setTestModeEnabled(false);
}

QString SessionBrokerStartupContractTest::createBinary(const QString &name, const QByteArray &content)
{
    const QString path = m_binDir + QStringLiteral("/") + name;
    return requireExecutable(path, content);
}

void SessionBrokerStartupContractTest::writeActiveConfig(const QString &compositor, const QString &shell)
{
    ConfigManager config;
    QString err;
    QVERIFY2(config.save(makeConfig(compositor, shell), &err), qPrintable(err));
}

QString SessionBrokerStartupContractTest::markerPath(const QString &name) const
{
    return m_tempDir.path() + QStringLiteral("/") + name;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    SessionBrokerStartupContractTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "sessionbroker_startup_contract_test.moc"
