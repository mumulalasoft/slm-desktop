#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>

#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmsessionstate.h"
#include "src/login/session-broker/sessionrollback.h"

using namespace Slm::Login;

namespace {

QJsonObject makeConfig(const QString &shell)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("compositor"), QStringLiteral("kwin_wayland"));
    obj.insert(QStringLiteral("compositorArgs"), QJsonArray{});
    obj.insert(QStringLiteral("shell"), shell);
    obj.insert(QStringLiteral("shellArgs"), QJsonArray{});
    return obj;
}

bool writeConfigFile(const QString &path, const QJsonObject &obj)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    const bool ok = (f.write(payload) == payload.size());
    f.close();
    return ok;
}

} // namespace

class SessionBrokerRecoveryRollbackTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void rollbackPrefersLastGoodSnapshot();
    void rollbackFallsBackToPreviousWhenSnapshotMissing();
    void rollbackFallsBackToSafeWhenSnapshotAndPreviousMissing();
};

void SessionBrokerRecoveryRollbackTest::init()
{
    QDir cfgDir(ConfigManager::configDir());
    if (cfgDir.exists()) {
        cfgDir.removeRecursively();
    }
    QVERIFY(QDir().mkpath(ConfigManager::configDir()));
}

void SessionBrokerRecoveryRollbackTest::rollbackPrefersLastGoodSnapshot()
{
    ConfigManager config;
    QVERIFY(config.load());

    QString err;
    QVERIFY(config.save(makeConfig(QStringLiteral("shell-snapshot")), &err));
    const QString snapshotId = config.snapshot(&err);
    QVERIFY2(!snapshotId.isEmpty(), qPrintable(err));

    QVERIFY(config.save(makeConfig(QStringLiteral("shell-prev")), &err));
    QVERIFY(config.save(makeConfig(QStringLiteral("shell-active")), &err));

    SessionState state;
    state.lastGoodSnapshot = snapshotId;

    const RecoveryRollbackSource source = rollbackRecoveryConfig(config, state, &err);
    QCOMPARE(source, RecoveryRollbackSource::LastGoodSnapshot);
    QVERIFY(config.load());
    QCOMPARE(config.shell(), QStringLiteral("shell-snapshot"));
}

void SessionBrokerRecoveryRollbackTest::rollbackFallsBackToPreviousWhenSnapshotMissing()
{
    ConfigManager config;
    QVERIFY(config.load());

    QString err;
    QVERIFY(config.save(makeConfig(QStringLiteral("shell-prev")), &err));
    QVERIFY(config.save(makeConfig(QStringLiteral("shell-active")), &err));

    SessionState state;
    state.lastGoodSnapshot = QStringLiteral("missing-snapshot-id");

    const RecoveryRollbackSource source = rollbackRecoveryConfig(config, state, &err);
    QCOMPARE(source, RecoveryRollbackSource::Previous);
    QVERIFY(config.load());
    QCOMPARE(config.shell(), QStringLiteral("shell-prev"));
}

void SessionBrokerRecoveryRollbackTest::rollbackFallsBackToSafeWhenSnapshotAndPreviousMissing()
{
    ConfigManager config;
    QVERIFY(config.load());

    QString err;
    QVERIFY(writeConfigFile(ConfigManager::safePath(), makeConfig(QStringLiteral("shell-safe"))));
    QVERIFY(writeConfigFile(ConfigManager::activePath(), makeConfig(QStringLiteral("shell-active"))));
    QFile::remove(ConfigManager::prevPath());

    SessionState state;
    state.lastGoodSnapshot = QStringLiteral("missing-snapshot-id");

    const RecoveryRollbackSource source = rollbackRecoveryConfig(config, state, &err);
    QCOMPARE(source, RecoveryRollbackSource::Safe);
    QVERIFY(config.load());
    QCOMPARE(config.shell(), QStringLiteral("shell-safe"));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString cfgRoot = QStringLiteral("/tmp/slm-login-tests-recovery-rollback-%1")
                                .arg(QCoreApplication::applicationPid());
    QDir().mkpath(cfgRoot);
    qputenv("XDG_CONFIG_HOME", cfgRoot.toLocal8Bit());
    QStandardPaths::setTestModeEnabled(false);
    SessionBrokerRecoveryRollbackTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "sessionbroker_recovery_rollback_test.moc"
