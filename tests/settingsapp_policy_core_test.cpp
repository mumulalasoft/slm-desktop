#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QSignalSpy>
#include <QStandardPaths>

#include "../src/apps/settings/settingsapp.h"

namespace {
QString policyModuleDir()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return appData + QStringLiteral("/modules/policy-test");
}

void clearPolicyTestModule()
{
    QDir(policyModuleDir()).removeRecursively();
}

QString writePolicyTestModule()
{
    const QString moduleDir = policyModuleDir();
    QDir().mkpath(moduleDir);

    QFile meta(moduleDir + QStringLiteral("/metadata.json"));
    if (meta.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QByteArray json = R"json(
{
  "id": "policy-test",
  "name": "Policy Test",
  "icon": "preferences-system",
  "group": "Testing",
  "description": "Policy resolver test module.",
  "keywords": ["policy","test"],
  "main": "PolicyPage.qml",
  "settings": [
    {
      "id": "danger",
      "label": "Danger Setting",
      "description": "Requires privileged action.",
      "type": "toggle",
      "backend_binding": "mock:false",
      "privileged_action": "org.slm.settings.test.danger"
    }
  ]
}
)json";
        meta.write(json);
    }

    QFile qml(moduleDir + QStringLiteral("/PolicyPage.qml"));
    if (qml.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qml.write("import QtQuick 2.15\nItem {}\n");
    }
    return moduleDir;
}

void clearGrantStore()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile::remove(dir + QStringLiteral("/settings-permissions.ini"));
}
}

class SettingsAppPolicyCoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        clearGrantStore();
        clearPolicyTestModule();
        qunsetenv("SLM_SETTINGS_AUTH_ALLOW_ALL");
    }

    void cleanup()
    {
        clearGrantStore();
        clearPolicyTestModule();
        qunsetenv("SLM_SETTINGS_AUTH_ALLOW_ALL");
    }

    void denyAlways_cachedAsImmediateDeny()
    {
        writePolicyTestModule();
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QSignalSpy spy(&app, SIGNAL(settingAuthorizationFinished(QString,QString,QString,bool,QString)));

        const QString requestId = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                              QStringLiteral("danger"),
                                                                              QStringLiteral("deny-always"));
        QVERIFY(!requestId.isEmpty());
        QTRY_COMPARE(spy.count(), 1);
        {
            const QList<QVariant> args = spy.takeFirst();
            QCOMPARE(args.value(3).toBool(), false);
            QCOMPARE(args.value(4).toString(), QStringLiteral("deny-always"));
        }

        const QVariantMap grantState = app.settingGrantState(QStringLiteral("policy-test"),
                                                             QStringLiteral("danger"));
        QCOMPARE(grantState.value(QStringLiteral("denyAlways")).toBool(), true);
        QCOMPARE(grantState.value(QStringLiteral("allowAlways")).toBool(), false);

        const QString requestId2 = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                               QStringLiteral("danger"),
                                                                               QStringLiteral("allow-once"));
        QVERIFY(!requestId2.isEmpty());
        QTRY_COMPARE(spy.count(), 1);
        const QList<QVariant> args2 = spy.takeFirst();
        QCOMPARE(args2.value(3).toBool(), false);
        QCOMPARE(args2.value(4).toString(), QStringLiteral("deny-always"));
    }

    void clearSettingGrant_removesCachedDecision()
    {
        writePolicyTestModule();
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QSignalSpy spy(&app, SIGNAL(settingAuthorizationFinished(QString,QString,QString,bool,QString)));

        const QString denied = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                           QStringLiteral("danger"),
                                                                           QStringLiteral("deny-always"));
        QVERIFY(!denied.isEmpty());
        QTRY_COMPARE(spy.count(), 1);
        spy.clear();

        QVERIFY(app.clearSettingGrant(QStringLiteral("policy-test"), QStringLiteral("danger")));
        const QVariantMap state = app.settingGrantState(QStringLiteral("policy-test"), QStringLiteral("danger"));
        QCOMPARE(state.value(QStringLiteral("allowAlways")).toBool(), false);
        QCOMPARE(state.value(QStringLiteral("denyAlways")).toBool(), false);

        const QString afterClear = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                               QStringLiteral("danger"),
                                                                               QStringLiteral("deny"));
        QVERIFY(!afterClear.isEmpty());
        QTRY_COMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.value(4).toString(), QStringLiteral("deny"));
    }

    void listSettingGrants_returnsStoredRows()
    {
        writePolicyTestModule();
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QSignalSpy spy(&app, SIGNAL(settingAuthorizationFinished(QString,QString,QString,bool,QString)));

        const QString r1 = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                       QStringLiteral("danger"),
                                                                       QStringLiteral("deny-always"));
        QVERIFY(!r1.isEmpty());
        QTRY_COMPARE(spy.count(), 1);
        spy.clear();

        const QVariantList rows = app.listSettingGrants();
        QVERIFY(!rows.isEmpty());
        bool foundDeny = false;
        qint64 foundUpdatedAt = 0;
        for (const QVariant &rowVar : rows) {
            const QVariantMap row = rowVar.toMap();
            if (row.value(QStringLiteral("moduleId")).toString() == QStringLiteral("policy-test")
                && row.value(QStringLiteral("settingId")).toString() == QStringLiteral("danger")
                && row.value(QStringLiteral("decision")).toString() == QStringLiteral("deny-always")) {
                foundDeny = true;
                foundUpdatedAt = row.value(QStringLiteral("updatedAt")).toLongLong();
                break;
            }
        }
        QVERIFY(foundDeny);
        QVERIFY(foundUpdatedAt > 0);
    }

    void grantsChanged_emittedOnGrantMutation()
    {
        writePolicyTestModule();
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QSignalSpy authSpy(&app, SIGNAL(settingAuthorizationFinished(QString,QString,QString,bool,QString)));
        QSignalSpy grantsSpy(&app, SIGNAL(grantsChanged()));

        const QString r1 = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                       QStringLiteral("danger"),
                                                                       QStringLiteral("deny-always"));
        QVERIFY(!r1.isEmpty());
        QTRY_COMPARE(authSpy.count(), 1);
        QVERIFY(grantsSpy.count() >= 1);
        grantsSpy.clear();

        app.clearSettingGrant(QStringLiteral("policy-test"), QStringLiteral("danger"));
        QVERIFY(grantsSpy.count() >= 1);
    }
};

QTEST_GUILESS_MAIN(SettingsAppPolicyCoreTest)
#include "settingsapp_policy_core_test.moc"
