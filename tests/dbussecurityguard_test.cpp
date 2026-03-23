#include <QtTest>

#include "../src/core/permissions/DBusSecurityGuard.h"
#include "../src/core/permissions/PermissionBroker.h"
#include "../src/core/permissions/PolicyEngine.h"
#include "../src/core/permissions/TrustResolver.h"

#include <QDBusMessage>

using namespace Slm::Permissions;

class DBusSecurityGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void deniesThirdPartyFileActionWithoutGesture();
    void allowsThirdPartyLowRiskSearchQuery();
};

void DBusSecurityGuardTest::deniesThirdPartyFileActionWithoutGesture()
{
    TrustResolver resolver;
    PolicyEngine engine;
    PermissionBroker broker;
    broker.setPolicyEngine(&engine);

    DBusSecurityGuard guard;
    guard.setTrustResolver(&resolver);
    guard.setPermissionBroker(&broker);

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.example.ThirdParty"),
        QStringLiteral("/org/example/Test"),
        QStringLiteral("org.example.Test"),
        QStringLiteral("Invoke"));

    const PolicyDecision decision = guard.check(
        msg,
        Capability::FileActionInvoke,
        QVariantMap{
            {QStringLiteral("resourceType"), QStringLiteral("action")},
            {QStringLiteral("resourceId"), QStringLiteral("test-action")},
            {QStringLiteral("initiatedByUserGesture"), false},
            {QStringLiteral("initiatedFromOfficialUI"), false},
            {QStringLiteral("sensitivityLevel"), QStringLiteral("Medium")},
        });

    QVERIFY(!decision.isAllowed());
    QCOMPARE(decision.type, DecisionType::Deny);
}

void DBusSecurityGuardTest::allowsThirdPartyLowRiskSearchQuery()
{
    TrustResolver resolver;
    PolicyEngine engine;
    PermissionBroker broker;
    broker.setPolicyEngine(&engine);

    DBusSecurityGuard guard;
    guard.setTrustResolver(&resolver);
    guard.setPermissionBroker(&broker);

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.example.ThirdParty"),
        QStringLiteral("/org/example/Test"),
        QStringLiteral("org.example.Test"),
        QStringLiteral("Query"));

    const PolicyDecision decision = guard.check(
        msg,
        Capability::SearchQueryFiles,
        QVariantMap{
            {QStringLiteral("resourceType"), QStringLiteral("search-query")},
            {QStringLiteral("resourceId"), QStringLiteral("search")},
            {QStringLiteral("sensitivityLevel"), QStringLiteral("Low")},
        });

    QVERIFY(decision.isAllowed());
    QCOMPARE(decision.type, DecisionType::Allow);
}

QTEST_MAIN(DBusSecurityGuardTest)
#include "dbussecurityguard_test.moc"

