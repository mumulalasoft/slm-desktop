#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>

#include "src/login/libslmlogin/slmconfigmanager.h"

using namespace Slm::Login;

class SlmConfigManagerValidationTest : public QObject
{
    Q_OBJECT

private slots:
    void saveRejectsInvalidTypes();
    void loadFallsBackToSafeWhenActiveInvalid();
    void snapshotRestoreRoundTrip();
};

void SlmConfigManagerValidationTest::saveRejectsInvalidTypes()
{
    ConfigManager mgr;
    mgr.load();

    QJsonObject bad;
    bad.insert(QStringLiteral("compositor"), 7);
    bad.insert(QStringLiteral("shell"), QStringLiteral("slm-shell"));
    bad.insert(QStringLiteral("compositorArgs"), QJsonArray{QStringLiteral("--flag")});
    bad.insert(QStringLiteral("shellArgs"), QJsonArray{QStringLiteral("--test")});

    QString err;
    QVERIFY(!mgr.save(bad, &err));
    QVERIFY(!err.isEmpty());
}

void SlmConfigManagerValidationTest::loadFallsBackToSafeWhenActiveInvalid()
{
    ConfigManager mgr;
    const QString active = ConfigManager::activePath();
    const QString safe = ConfigManager::safePath();

    {
        QFile f(active);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray badJson = "{\"compositor\": 99}";
        QCOMPARE(f.write(badJson), badJson.size());
        f.close();
    }

    {
        QJsonObject good;
        good.insert(QStringLiteral("compositor"), QStringLiteral("kwin_wayland"));
        good.insert(QStringLiteral("compositorArgs"), QJsonArray{});
        good.insert(QStringLiteral("shell"), QStringLiteral("slm-shell"));
        good.insert(QStringLiteral("shellArgs"), QJsonArray{});
        QFile f(safe);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray payload = QJsonDocument(good).toJson(QJsonDocument::Indented);
        QCOMPARE(f.write(payload), payload.size());
        f.close();
    }

    QVERIFY(mgr.load());
    QCOMPARE(mgr.compositor(), QStringLiteral("kwin_wayland"));
    QCOMPARE(mgr.shell(), QStringLiteral("slm-shell"));
}

void SlmConfigManagerValidationTest::snapshotRestoreRoundTrip()
{
    ConfigManager mgr;
    QVERIFY(mgr.load());

    QJsonObject stable;
    stable.insert(QStringLiteral("compositor"), QStringLiteral("kwin_wayland"));
    stable.insert(QStringLiteral("compositorArgs"), QJsonArray{QStringLiteral("--xwayland")});
    stable.insert(QStringLiteral("shell"), QStringLiteral("slm-shell"));
    stable.insert(QStringLiteral("shellArgs"), QJsonArray{QStringLiteral("--stable")});

    QString err;
    QVERIFY2(mgr.save(stable, &err), qPrintable(err));

    const QString snapshotId = mgr.snapshot(&err);
    QVERIFY2(!snapshotId.isEmpty(), qPrintable(err));

    QJsonObject risky = stable;
    risky.insert(QStringLiteral("shell"), QStringLiteral("slm-shell-dev"));
    QVERIFY2(mgr.save(risky, &err), qPrintable(err));
    QCOMPARE(mgr.shell(), QStringLiteral("slm-shell-dev"));

    QVERIFY2(mgr.restoreSnapshot(snapshotId, &err), qPrintable(err));
    QVERIFY(mgr.load());
    QCOMPARE(mgr.shell(), QStringLiteral("slm-shell"));
    QCOMPARE(mgr.shellArgs(), QStringList{QStringLiteral("--stable")});
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString cfgRoot = QStringLiteral("/tmp/slm-login-tests-configmanager-%1")
                                .arg(QCoreApplication::applicationPid());
    QDir().mkpath(cfgRoot);
    qputenv("XDG_CONFIG_HOME", cfgRoot.toLocal8Bit());
    QStandardPaths::setTestModeEnabled(false);
    SlmConfigManagerValidationTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "slmconfigmanager_validation_test.moc"
