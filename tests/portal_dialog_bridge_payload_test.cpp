#include <QtTest/QtTest>

#include "../src/core/permissions/AccessContext.h"
#include "../src/core/permissions/Capability.h"
#include "../src/core/permissions/CallerIdentity.h"
#include "../src/daemon/portald/portal-adapter/PortalDialogBridge.h"

class PortalDialogBridgePayloadTest : public QObject
{
    Q_OBJECT

private slots:
    void requestConsent_payloadCarriesPersistentEligibility();
    void submitConsent_invokesPendingCallback();
};

void PortalDialogBridgePayloadTest::requestConsent_payloadCarriesPersistentEligibility()
{
    Slm::PortalAdapter::PortalDialogBridge bridge;
    QSignalSpy spy(&bridge, &Slm::PortalAdapter::PortalDialogBridge::consentRequested);
    QVERIFY(spy.isValid());

    Slm::Permissions::CallerIdentity caller;
    caller.appId = QStringLiteral("org.example.Test");
    caller.desktopFileId = QStringLiteral("org.example.Test.desktop");
    caller.busName = QStringLiteral(":1.777");

    Slm::Permissions::AccessContext context;
    context.resourceType = QStringLiteral("secret");
    context.resourceId = QStringLiteral("token");
    context.sensitivityLevel = Slm::Permissions::SensitivityLevel::High;
    context.initiatedByUserGesture = true;

    bool callbackCalled = false;
    bridge.requestConsent(QStringLiteral("/request/secret-payload"),
                          caller,
                          Slm::Permissions::Capability::SecretDelete,
                          context,
                          false,
                          [&callbackCalled](const Slm::PortalAdapter::ConsentResult &) { callbackCalled = true; });

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.size(), 2);
    QCOMPARE(args.at(0).toString(), QStringLiteral("/request/secret-payload"));
    const QVariantMap payload = args.at(1).toMap();
    QCOMPARE(payload.value(QStringLiteral("appId")).toString(), caller.appId);
    QCOMPARE(payload.value(QStringLiteral("desktopFileId")).toString(), caller.desktopFileId);
    QCOMPARE(payload.value(QStringLiteral("busName")).toString(), caller.busName);
    QCOMPARE(payload.value(QStringLiteral("capability")).toString(), QStringLiteral("Secret.Delete"));
    QCOMPARE(payload.value(QStringLiteral("resourceType")).toString(), QStringLiteral("secret"));
    QCOMPARE(payload.value(QStringLiteral("resourceId")).toString(), QStringLiteral("token"));
    QCOMPARE(payload.value(QStringLiteral("persistentEligible")).toBool(), false);
    QVERIFY(!callbackCalled);
}

void PortalDialogBridgePayloadTest::submitConsent_invokesPendingCallback()
{
    Slm::PortalAdapter::PortalDialogBridge bridge;
    QSignalSpy spy(&bridge, &Slm::PortalAdapter::PortalDialogBridge::consentRequested);
    QVERIFY(spy.isValid());

    Slm::Permissions::CallerIdentity caller;
    caller.appId = QStringLiteral("org.example.Test");
    Slm::Permissions::AccessContext context;
    context.resourceType = QStringLiteral("secret");
    context.resourceId = QStringLiteral("token");

    bool callbackCalled = false;
    Slm::PortalAdapter::ConsentResult callbackResult;
    bridge.requestConsent(QStringLiteral("/request/secret-callback"),
                          caller,
                          Slm::Permissions::Capability::SecretRead,
                          context,
                          true,
                          [&callbackCalled, &callbackResult](const Slm::PortalAdapter::ConsentResult &result) {
                              callbackCalled = true;
                              callbackResult = result;
                          });

    QCOMPARE(spy.count(), 1);
    bridge.submitConsent(QStringLiteral("/request/secret-callback"),
                         QStringLiteral("allow_always"),
                         true,
                         QStringLiteral("persistent"),
                         QStringLiteral("approved"));

    QVERIFY(callbackCalled);
    QCOMPARE(callbackResult.decision, Slm::PortalAdapter::UserDecision::AllowAlways);
    QCOMPARE(callbackResult.persist, true);
    QCOMPARE(callbackResult.scope, QStringLiteral("persistent"));
    QCOMPARE(callbackResult.reason, QStringLiteral("approved"));
}

QTEST_MAIN(PortalDialogBridgePayloadTest)
#include "portal_dialog_bridge_payload_test.moc"
