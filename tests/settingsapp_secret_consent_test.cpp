#include <QtTest/QtTest>

#include <QQmlApplicationEngine>
#include <QStringList>

#include "../src/apps/settings/settingsapp.h"

class SettingsAppSecretConsentTest : public QObject
{
    Q_OBJECT

private slots:
    void listSecretConsentSummary_groupsByApp_nonDbus()
    {
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);

        app.setPortalInvokerForTests([](const QString &method, const QVariantList &) -> QVariantMap {
            if (method == QStringLiteral("ListSecretConsentGrants")) {
                return {
                    {QStringLiteral("ok"), true},
                    {QStringLiteral("items"), QVariantList{
                         QVariantMap{
                             {QStringLiteral("appId"), QStringLiteral("org.demo.app")},
                             {QStringLiteral("capability"), QStringLiteral("Secret.Read")},
                             {QStringLiteral("updatedAt"), 1000ll},
                         },
                         QVariantMap{
                             {QStringLiteral("appId"), QStringLiteral("org.demo.app")},
                             {QStringLiteral("capability"), QStringLiteral("Secret.Store")},
                             {QStringLiteral("updatedAt"), 2000ll},
                         },
                         QVariantMap{
                             {QStringLiteral("appId"), QStringLiteral("org.other.app")},
                             {QStringLiteral("capability"), QStringLiteral("Secret.Delete")},
                             {QStringLiteral("updatedAt"), 1500ll},
                         },
                     }},
                };
            }
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("unexpected-method")},
                {QStringLiteral("method"), method},
            };
        });

        const QVariantList rows = app.listSecretConsentSummary();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).toMap().value(QStringLiteral("appId")).toString(), QStringLiteral("org.demo.app"));
        QCOMPARE(rows.at(0).toMap().value(QStringLiteral("grantCount")).toInt(), 2);
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("appId")).toString(), QStringLiteral("org.other.app"));
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("grantCount")).toInt(), 1);
    }

    void revokeThenClear_callsPortalMethodsInOrder_nonDbus()
    {
        QQmlApplicationEngine engine;
        SettingsApp app(&engine);

        QStringList methodCalls;
        app.setPortalInvokerForTests([&methodCalls](const QString &method, const QVariantList &args) -> QVariantMap {
            methodCalls.push_back(method);
            const QVariantMap payload = args.value(0).toMap();

            if (method == QStringLiteral("RevokeSecretConsentGrants")) {
                return {
                    {QStringLiteral("ok"), payload.value(QStringLiteral("appId")).toString() == QStringLiteral("org.demo.app")},
                    {QStringLiteral("revokeCount"), 2},
                };
            }
            if (method == QStringLiteral("ClearAppSecrets")) {
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

        const QVariantMap revoke = app.revokeSecretConsentForApp(QStringLiteral("org.demo.app"));
        QVERIFY(revoke.value(QStringLiteral("ok")).toBool());
        QCOMPARE(revoke.value(QStringLiteral("revokeCount")).toInt(), 2);

        const QVariantMap clear = app.clearSecretDataForApp(QStringLiteral("org.demo.app"));
        QVERIFY(clear.value(QStringLiteral("ok")).toBool());

        QCOMPARE(methodCalls,
                 QStringList({QStringLiteral("RevokeSecretConsentGrants"),
                              QStringLiteral("ClearAppSecrets")}));
    }
};

QTEST_GUILESS_MAIN(SettingsAppSecretConsentTest)
#include "settingsapp_secret_consent_test.moc"
