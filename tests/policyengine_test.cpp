#include <QtTest>

#include "../src/core/permissions/PolicyEngine.h"

using namespace Slm::Permissions;

class PolicyEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void evaluatePolicyMatrix_data();
    void evaluatePolicyMatrix();
    void unknownCapabilityDenied();
};

void PolicyEngineTest::evaluatePolicyMatrix_data()
{
    QTest::addColumn<int>("trustLevel");
    QTest::addColumn<int>("capability");
    QTest::addColumn<bool>("gesture");
    QTest::addColumn<bool>("officialUi");
    QTest::addColumn<int>("expectedDecision");

    // Third-party low-risk search access allowed.
    QTest::newRow("thirdparty-search-files-allow")
        << static_cast<int>(TrustLevel::ThirdPartyApplication)
        << static_cast<int>(Capability::SearchQueryFiles)
        << false
        << false
        << static_cast<int>(DecisionType::Allow);

    // Third-party file action requires gesture, deny when missing.
    QTest::newRow("thirdparty-fileaction-deny-no-gesture")
        << static_cast<int>(TrustLevel::ThirdPartyApplication)
        << static_cast<int>(Capability::FileActionInvoke)
        << false
        << false
        << static_cast<int>(DecisionType::Deny);

    // Third-party file action with gesture should ask user.
    QTest::newRow("thirdparty-fileaction-ask-with-gesture")
        << static_cast<int>(TrustLevel::ThirdPartyApplication)
        << static_cast<int>(Capability::FileActionInvoke)
        << true
        << false
        << static_cast<int>(DecisionType::AskUser);

    // Core component gesture-gated capability asks without gesture.
    QTest::newRow("core-share-ask-no-gesture")
        << static_cast<int>(TrustLevel::CoreDesktopComponent)
        << static_cast<int>(Capability::ShareInvoke)
        << false
        << false
        << static_cast<int>(DecisionType::AskUser);

    // Core component allowed when gesture present.
    QTest::newRow("core-share-allow-with-gesture")
        << static_cast<int>(TrustLevel::CoreDesktopComponent)
        << static_cast<int>(Capability::ShareInvoke)
        << true
        << false
        << static_cast<int>(DecisionType::Allow);

    // Core component allowed when explicitly from official UI.
    QTest::newRow("core-share-allow-official-ui")
        << static_cast<int>(TrustLevel::CoreDesktopComponent)
        << static_cast<int>(Capability::ShareInvoke)
        << false
        << true
        << static_cast<int>(DecisionType::Allow);

    // Privileged non-official mail body requires mediation.
    QTest::newRow("privileged-mail-body-ask")
        << static_cast<int>(TrustLevel::PrivilegedDesktopService)
        << static_cast<int>(Capability::AccountsReadMailBody)
        << true
        << false
        << static_cast<int>(DecisionType::AskUser);

    // Privileged service regular capability allowed.
    QTest::newRow("privileged-query-files-allow")
        << static_cast<int>(TrustLevel::PrivilegedDesktopService)
        << static_cast<int>(Capability::SearchQueryFiles)
        << false
        << false
        << static_cast<int>(DecisionType::Allow);

    // Third-party screenshot area with gesture should ask user.
    QTest::newRow("thirdparty-screenshot-area-ask-with-gesture")
        << static_cast<int>(TrustLevel::ThirdPartyApplication)
        << static_cast<int>(Capability::ScreenshotCaptureArea)
        << true
        << false
        << static_cast<int>(DecisionType::AskUser);

    // Third-party global shortcuts create session with gesture should ask user.
    QTest::newRow("thirdparty-shortcuts-create-ask-with-gesture")
        << static_cast<int>(TrustLevel::ThirdPartyApplication)
        << static_cast<int>(Capability::GlobalShortcutsCreateSession)
        << true
        << false
        << static_cast<int>(DecisionType::AskUser);

    // Third-party global shortcuts deactivate is low-risk allow.
    QTest::newRow("thirdparty-shortcuts-deactivate-allow")
        << static_cast<int>(TrustLevel::ThirdPartyApplication)
        << static_cast<int>(Capability::GlobalShortcutsDeactivate)
        << false
        << false
        << static_cast<int>(DecisionType::Allow);
}

void PolicyEngineTest::evaluatePolicyMatrix()
{
    QFETCH(int, trustLevel);
    QFETCH(int, capability);
    QFETCH(bool, gesture);
    QFETCH(bool, officialUi);
    QFETCH(int, expectedDecision);

    PolicyEngine engine;
    CallerIdentity caller;
    caller.trustLevel = static_cast<TrustLevel>(trustLevel);
    caller.isOfficialComponent = (caller.trustLevel == TrustLevel::CoreDesktopComponent);

    AccessContext context;
    context.capability = static_cast<Capability>(capability);
    context.initiatedByUserGesture = gesture;
    context.initiatedFromOfficialUI = officialUi;
    context.sensitivityLevel = engine.sensitivityForCapability(context.capability);

    const PolicyDecision decision = engine.evaluate(caller, context);
    QCOMPARE(static_cast<int>(decision.type), expectedDecision);
}

void PolicyEngineTest::unknownCapabilityDenied()
{
    PolicyEngine engine;
    CallerIdentity caller;
    caller.trustLevel = TrustLevel::CoreDesktopComponent;
    caller.isOfficialComponent = true;

    AccessContext context;
    context.capability = Capability::Unknown;
    context.sensitivityLevel = SensitivityLevel::Low;

    const PolicyDecision decision = engine.evaluate(caller, context);
    QCOMPARE(decision.type, DecisionType::Deny);
    QCOMPARE(decision.reason, QStringLiteral("unknown-capability"));
}

QTEST_MAIN(PolicyEngineTest)
#include "policyengine_test.moc"
