#include <QtTest/QtTest>

#include <QDir>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QSignalSpy>
#include <QStandardPaths>

#include "../src/apps/settings/settingsapp.h"

namespace {
QString writePolicyTestModule()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString moduleDir = appData + QStringLiteral("/modules/policy-test");
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

class FakePortalServiceForSettings : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Portal")

public:
    QString lastRevokedAppId;
    QString lastClearedAppId;
    int revokeCallCount = 0;
    int clearCallCount = 0;

public slots:
    QVariantMap ListSecretConsentGrants(const QVariantMap &)
    {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("items"), QVariantList{
                 QVariantMap{
                     {QStringLiteral("appId"), QStringLiteral("org.demo.app")},
                     {QStringLiteral("capability"), QStringLiteral("Secret.Read")},
                     {QStringLiteral("decision"), QStringLiteral("allow-always")},
                     {QStringLiteral("updatedAt"), 1000ll},
                 },
                 QVariantMap{
                     {QStringLiteral("appId"), QStringLiteral("org.demo.app")},
                     {QStringLiteral("capability"), QStringLiteral("Secret.Store")},
                     {QStringLiteral("decision"), QStringLiteral("allow-always")},
                     {QStringLiteral("updatedAt"), 2000ll},
                 },
                 QVariantMap{
                     {QStringLiteral("appId"), QStringLiteral("org.other.app")},
                     {QStringLiteral("capability"), QStringLiteral("Secret.Delete")},
                     {QStringLiteral("decision"), QStringLiteral("deny-always")},
                     {QStringLiteral("updatedAt"), 1500ll},
                 },
             }},
        };
    }

    QVariantMap RevokeSecretConsentGrants(const QVariantMap &options)
    {
        lastRevokedAppId = options.value(QStringLiteral("appId")).toString();
        ++revokeCallCount;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("appId"), lastRevokedAppId},
            {QStringLiteral("revokeCount"), 2},
            {QStringLiteral("failedCount"), 0},
        };
    }

    QVariantMap ClearAppSecrets(const QVariantMap &options)
    {
        lastClearedAppId = options.value(QStringLiteral("appId")).toString();
        ++clearCallCount;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("appId"), lastClearedAppId},
        };
    }
};
}

class SettingsAppPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        clearGrantStore();
    }

    void cleanup()
    {
        clearGrantStore();
        qunsetenv("SLM_SETTINGS_AUTH_ALLOW_ALL");
    }

    void metadataPrivilegedAction_exposedInSettingPolicy()
    {
        const QString moduleDir = writePolicyTestModule();
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);

        const QVariantMap policy = app.settingPolicy(QStringLiteral("policy-test"),
                                                     QStringLiteral("danger"));
        QCOMPARE(policy.value(QStringLiteral("privilegedAction")).toString(),
                 QStringLiteral("org.slm.settings.test.danger"));
        QCOMPARE(policy.value(QStringLiteral("requiresAuthorization")).toBool(), true);
        QCOMPARE(policy.value(QStringLiteral("policySource")).toString(),
                 QStringLiteral("module-metadata"));

        QDir(moduleDir).removeRecursively();
    }

    void defaultPolicyFallback_appliesForKnownSetting()
    {
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);

        const QVariantMap policy = app.settingPolicy(QStringLiteral("network"),
                                                     QStringLiteral("wifi"));
        QCOMPARE(policy.value(QStringLiteral("privilegedAction")).toString(),
                 QStringLiteral("org.slm.settings.network.modify"));
        QCOMPARE(policy.value(QStringLiteral("requiresAuthorization")).toBool(), true);
        QCOMPARE(policy.value(QStringLiteral("policySource")).toString(),
                 QStringLiteral("default-policy"));
    }

    void requestAuthorization_devOverrideAllows()
    {
        const QString moduleDir = writePolicyTestModule();
        qputenv("SLM_SETTINGS_AUTH_ALLOW_ALL", "1");

        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QSignalSpy spy(&app, SIGNAL(settingAuthorizationFinished(QString,QString,QString,bool,QString)));

        const QString requestId = app.requestSettingAuthorization(QStringLiteral("policy-test"),
                                                                  QStringLiteral("danger"));
        QVERIFY(!requestId.isEmpty());
        QTRY_COMPARE(spy.count(), 1);

        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.value(0).toString(), requestId);
        QCOMPARE(args.value(1).toString(), QStringLiteral("policy-test"));
        QCOMPARE(args.value(2).toString(), QStringLiteral("danger"));
        QCOMPARE(args.value(3).toBool(), true);
        QCOMPARE(args.value(4).toString(), QStringLiteral("dev-override-allow-all"));

        QDir(moduleDir).removeRecursively();
    }

    void denyAlways_cachedAsImmediateDeny()
    {
        const QString moduleDir = writePolicyTestModule();
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

        QDir(moduleDir).removeRecursively();
    }

    void allowAlways_cachedAfterSuccessfulAuthorization()
    {
        const QString moduleDir = writePolicyTestModule();
        qputenv("SLM_SETTINGS_AUTH_ALLOW_ALL", "1");

        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QSignalSpy spy(&app, SIGNAL(settingAuthorizationFinished(QString,QString,QString,bool,QString)));

        const QString requestId = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                              QStringLiteral("danger"),
                                                                              QStringLiteral("allow-always"));
        QVERIFY(!requestId.isEmpty());
        QTRY_COMPARE(spy.count(), 1);
        {
            const QList<QVariant> args = spy.takeFirst();
            QCOMPARE(args.value(3).toBool(), true);
            QCOMPARE(args.value(4).toString(), QStringLiteral("dev-override-allow-all"));
        }

        qunsetenv("SLM_SETTINGS_AUTH_ALLOW_ALL");
        const QVariantMap grantState = app.settingGrantState(QStringLiteral("policy-test"),
                                                             QStringLiteral("danger"));
        QCOMPARE(grantState.value(QStringLiteral("allowAlways")).toBool(), true);
        QCOMPARE(grantState.value(QStringLiteral("denyAlways")).toBool(), false);

        const QString requestId2 = app.requestSettingAuthorizationWithDecision(QStringLiteral("policy-test"),
                                                                               QStringLiteral("danger"),
                                                                               QStringLiteral("allow-once"));
        QVERIFY(!requestId2.isEmpty());
        QTRY_COMPARE(spy.count(), 1);
        const QList<QVariant> args2 = spy.takeFirst();
        QCOMPARE(args2.value(3).toBool(), true);
        QCOMPARE(args2.value(4).toString(), QStringLiteral("allow-always-cached"));

        QDir(moduleDir).removeRecursively();
    }

    void clearSettingGrant_removesCachedDecision()
    {
        const QString moduleDir = writePolicyTestModule();
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

        QDir(moduleDir).removeRecursively();
    }

    void listSettingGrants_returnsStoredRows()
    {
        const QString moduleDir = writePolicyTestModule();
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

        QDir(moduleDir).removeRecursively();
    }

    void grantsChanged_emittedOnGrantMutation()
    {
        const QString moduleDir = writePolicyTestModule();
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

        QDir(moduleDir).removeRecursively();
    }

    void secretConsentSummary_andRevoke_fromPortal()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QStringLiteral("org.slm.Desktop.Portal")).value()) {
            QSKIP("org.slm.Desktop.Portal already owned by another process");
        }

        FakePortalServiceForSettings fakePortal;
        QVERIFY(bus.registerService(QStringLiteral("org.slm.Desktop.Portal")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/slm/Desktop/Portal"),
                                   &fakePortal,
                                   QDBusConnection::ExportAllSlots));

        QQmlApplicationEngine engine;
        SettingsApp app(&engine);

        const QVariantList summary = app.listSecretConsentSummary();
        QCOMPARE(summary.size(), 2);
        QCOMPARE(summary.at(0).toMap().value(QStringLiteral("appId")).toString(),
                 QStringLiteral("org.demo.app"));
        QCOMPARE(summary.at(0).toMap().value(QStringLiteral("grantCount")).toInt(), 2);
        QCOMPARE(summary.at(1).toMap().value(QStringLiteral("appId")).toString(),
                 QStringLiteral("org.other.app"));
        QCOMPARE(summary.at(1).toMap().value(QStringLiteral("grantCount")).toInt(), 1);

        const QVariantMap revoke = app.revokeSecretConsentForApp(QStringLiteral("org.demo.app"));
        QVERIFY(revoke.value(QStringLiteral("ok")).toBool());
        QCOMPARE(revoke.value(QStringLiteral("revokeCount")).toInt(), 2);
        QCOMPARE(fakePortal.lastRevokedAppId, QStringLiteral("org.demo.app"));

        bus.unregisterObject(QStringLiteral("/org/slm/Desktop/Portal"));
        bus.unregisterService(QStringLiteral("org.slm.Desktop.Portal"));
    }

    void secretConsentRevoke_thenClear_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface unavailable");
        }
        if (iface->isServiceRegistered(QStringLiteral("org.slm.Desktop.Portal")).value()) {
            QSKIP("org.slm.Desktop.Portal already owned by another process");
        }

        FakePortalServiceForSettings fakePortal;
        QVERIFY(bus.registerService(QStringLiteral("org.slm.Desktop.Portal")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/slm/Desktop/Portal"),
                                   &fakePortal,
                                   QDBusConnection::ExportAllSlots));

        QQmlApplicationEngine engine;
        SettingsApp app(&engine);

        const QVariantMap revoke = app.revokeSecretConsentForApp(QStringLiteral("org.demo.app"));
        QVERIFY(revoke.value(QStringLiteral("ok")).toBool());
        QCOMPARE(fakePortal.revokeCallCount, 1);
        QCOMPARE(fakePortal.lastRevokedAppId, QStringLiteral("org.demo.app"));

        const QVariantMap clear = app.clearSecretDataForApp(QStringLiteral("org.demo.app"));
        QVERIFY(clear.value(QStringLiteral("ok")).toBool());
        QCOMPARE(fakePortal.clearCallCount, 1);
        QCOMPARE(fakePortal.lastClearedAppId, QStringLiteral("org.demo.app"));

        bus.unregisterObject(QStringLiteral("/org/slm/Desktop/Portal"));
        bus.unregisterService(QStringLiteral("org.slm.Desktop.Portal"));
    }

    void secretConsentRevoke_thenClear_nonDbusInvoker_contract()
    {
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);

        QStringList methodCalls;
        app.setPortalInvokerForTests([&methodCalls](const QString &method, const QVariantList &args) -> QVariantMap {
            methodCalls.push_back(method);
            if (method == QStringLiteral("ListSecretConsentGrants")) {
                return {
                    {QStringLiteral("ok"), true},
                    {QStringLiteral("items"), QVariantList{
                         QVariantMap{
                             {QStringLiteral("appId"), QStringLiteral("org.demo.app")},
                             {QStringLiteral("capability"), QStringLiteral("Secret.Read")},
                             {QStringLiteral("updatedAt"), 42ll},
                         },
                     }},
                };
            }
            if (method == QStringLiteral("RevokeSecretConsentGrants")) {
                const QVariantMap payload = args.value(0).toMap();
                return {
                    {QStringLiteral("ok"), payload.value(QStringLiteral("appId")).toString() == QStringLiteral("org.demo.app")},
                    {QStringLiteral("revokeCount"), 1},
                };
            }
            if (method == QStringLiteral("ClearAppSecrets")) {
                const QVariantMap payload = args.value(0).toMap();
                return {
                    {QStringLiteral("ok"), payload.value(QStringLiteral("appId")).toString() == QStringLiteral("org.demo.app")},
                };
            }
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("unexpected-method")},
                {QStringLiteral("method"), method},
            };
        });

        const QVariantList summary = app.listSecretConsentSummary();
        QCOMPARE(summary.size(), 1);
        QCOMPARE(summary.constFirst().toMap().value(QStringLiteral("appId")).toString(),
                 QStringLiteral("org.demo.app"));
        QCOMPARE(summary.constFirst().toMap().value(QStringLiteral("grantCount")).toInt(), 1);

        const QVariantMap revoke = app.revokeSecretConsentForApp(QStringLiteral("org.demo.app"));
        QVERIFY(revoke.value(QStringLiteral("ok")).toBool());
        QCOMPARE(revoke.value(QStringLiteral("revokeCount")).toInt(), 1);

        const QVariantMap clear = app.clearSecretDataForApp(QStringLiteral("org.demo.app"));
        QVERIFY(clear.value(QStringLiteral("ok")).toBool());

        QCOMPARE(methodCalls, QStringList({
                         QStringLiteral("ListSecretConsentGrants"),
                         QStringLiteral("RevokeSecretConsentGrants"),
                         QStringLiteral("ClearAppSecrets"),
                     }));
    }
};

QTEST_GUILESS_MAIN(SettingsAppPolicyTest)
#include "settingsapp_policy_test.moc"
