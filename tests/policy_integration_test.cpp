#include <QtTest>

#include "../src/core/permissions/AuditLogger.h"
#include "../src/core/permissions/CallerIdentity.h"
#include "../src/core/permissions/ConsentMediator.h"
#include "../src/core/permissions/PermissionStore.h"
#include "../src/core/permissions/PolicyEngine.h"
#include "../src/core/permissions/TrustResolver.h"

#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Slm::Permissions;

// ── Helpers ───────────────────────────────────────────────────────────────────

static CallerIdentity makeCallerWithTrust(TrustLevel level, const QString &appId = {})
{
    CallerIdentity c;
    c.trustLevel        = level;
    c.isOfficialComponent = (level == TrustLevel::CoreDesktopComponent);
    c.appId             = appId.isEmpty() ? QStringLiteral("test.app") : appId;
    c.busName           = QStringLiteral("com.example.Test");
    return c;
}

static AccessContext makeContext(Capability cap,
                                 bool gesture = false,
                                 bool officialUi = false)
{
    PolicyEngine engine;
    AccessContext ctx;
    ctx.capability            = cap;
    ctx.initiatedByUserGesture = gesture;
    ctx.initiatedFromOfficialUI = officialUi;
    ctx.sensitivityLevel      = engine.sensitivityForCapability(cap);
    return ctx;
}

// ── Test class ────────────────────────────────────────────────────────────────

class PolicyIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    // Stored AllowAlways overrides AskUser from policy
    void storedAllowAlwaysOverridesAskUser();

    // Stored DenyAlways overrides Allow from policy
    void storedDenyAlwaysOverridesAllow();

    // No stored decision falls through to policy
    void noStoredDecisionFallsThroughToPolicy();

    // Audit logger writes on Allow decision
    void auditLoggerRecordsAllowDecision();

    // Audit logger writes on Deny decision
    void auditLoggerRecordsDenyDecision();

    // ConsentMediator: AllowOnce emits consentResolved with Allow
    void consentMediatorAllowOnceResolvesAllow();

    // ConsentMediator: DenyAlways persists to PermissionStore
    void consentMediatorDenyAlwaysPersists();

    // ConsentMediator: AllowAlways persists to PermissionStore
    void consentMediatorAllowAlwaysPersists();

    // ConsentMediator: timeout auto-denies
    void consentMediatorTimeoutDenies();

    // ConsentMediator: cancel resolves as DenyOnce, no persistence
    void consentMediatorCancelDoesNotPersist();

    // ConsentMediator: fulfilling unknown requestId is no-op
    void consentMediatorFulfillUnknownRequestIsNoOp();
};

// ── Stored permission override tests ─────────────────────────────────────────

void PolicyIntegrationTest::storedAllowAlwaysOverridesAskUser()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));
    QVERIFY(store.savePermission(
        QStringLiteral("test.app"),
        Capability::ShareInvoke,
        DecisionType::AllowAlways,
        QStringLiteral("persistent"),
        QStringLiteral("share"), QStringLiteral("all")));

    PolicyEngine engine;
    engine.setStore(&store);

    // Without store this would be AskUser (gesture-gated, no gesture).
    const CallerIdentity caller = makeCallerWithTrust(TrustLevel::ThirdPartyApplication,
                                                      QStringLiteral("test.app"));
    const AccessContext ctx = makeContext(Capability::ShareInvoke, /*gesture=*/true);

    const PolicyDecision decision = engine.evaluate(caller, ctx);
    // Stored AllowAlways should map to Allow.
    QCOMPARE(decision.type, DecisionType::Allow);
    QCOMPARE(decision.reason, QStringLiteral("stored-policy"));
}

void PolicyIntegrationTest::storedDenyAlwaysOverridesAllow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));
    QVERIFY(store.savePermission(
        QStringLiteral("test.app"),
        Capability::SearchQueryFiles,
        DecisionType::DenyAlways,
        QStringLiteral("persistent"),
        QStringLiteral("search-query"), QStringLiteral("all")));

    PolicyEngine engine;
    engine.setStore(&store);

    // Without store this is Always-Allow for third-party.
    const CallerIdentity caller = makeCallerWithTrust(TrustLevel::ThirdPartyApplication,
                                                      QStringLiteral("test.app"));
    const AccessContext ctx = makeContext(Capability::SearchQueryFiles);

    const PolicyDecision decision = engine.evaluate(caller, ctx);
    QCOMPARE(decision.type, DecisionType::Deny);
    QCOMPARE(decision.reason, QStringLiteral("stored-policy"));
}

void PolicyIntegrationTest::noStoredDecisionFallsThroughToPolicy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));
    // Nothing stored.

    PolicyEngine engine;
    engine.setStore(&store);

    const CallerIdentity caller = makeCallerWithTrust(TrustLevel::ThirdPartyApplication,
                                                      QStringLiteral("test.app"));
    const AccessContext ctx = makeContext(Capability::SearchQueryFiles);

    const PolicyDecision decision = engine.evaluate(caller, ctx);
    // Falls through to third-party-always-allow set.
    QCOMPARE(decision.type, DecisionType::Allow);
    QCOMPARE(decision.reason, QStringLiteral("third-party-low-risk"));
}

// ── AuditLogger tests ─────────────────────────────────────────────────────────

void PolicyIntegrationTest::auditLoggerRecordsAllowDecision()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));

    AuditLogger logger(&store);

    const CallerIdentity caller = makeCallerWithTrust(TrustLevel::CoreDesktopComponent);
    AccessContext ctx;
    ctx.capability   = Capability::SearchQueryFiles;
    ctx.resourceType = QStringLiteral("search-query");

    PolicyDecision decision;
    decision.type    = DecisionType::Allow;
    decision.reason  = QStringLiteral("core-component");
    decision.capability = Capability::SearchQueryFiles;

    logger.recordDecision(caller, ctx, decision);

    const QVariantList logs = store.listAuditLogs(10);
    QCOMPARE(logs.size(), 1);
    const QVariantMap row = logs.first().toMap();
    QCOMPARE(row.value(QStringLiteral("decision")).toString(), QStringLiteral("allow"));
    QCOMPARE(row.value(QStringLiteral("reason")).toString(), QStringLiteral("core-component"));
}

void PolicyIntegrationTest::auditLoggerRecordsDenyDecision()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));

    AuditLogger logger(&store);

    const CallerIdentity caller = makeCallerWithTrust(TrustLevel::ThirdPartyApplication);
    AccessContext ctx;
    ctx.capability   = Capability::FileActionInvoke;
    ctx.resourceType = QStringLiteral("action");

    PolicyDecision decision;
    decision.type    = DecisionType::Deny;
    decision.reason  = QStringLiteral("user-gesture-required");
    decision.capability = Capability::FileActionInvoke;

    logger.recordDecision(caller, ctx, decision);

    const QVariantList logs = store.listAuditLogs(10);
    QCOMPARE(logs.size(), 1);
    const QVariantMap row = logs.first().toMap();
    QCOMPARE(row.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
}

// ── ConsentMediator tests ─────────────────────────────────────────────────────

void PolicyIntegrationTest::consentMediatorAllowOnceResolvesAllow()
{
    ConsentMediator mediator;

    QSignalSpy spy(&mediator, &ConsentMediator::consentResolved);

    ConsentMediator::ConsentRequest req;
    req.appId      = QStringLiteral("com.example.App");
    req.capability = Capability::ShareInvoke;
    req.timeoutMs  = 5000;

    const QString id = mediator.requestConsent(req);
    QVERIFY(!id.isEmpty());
    QVERIFY(mediator.pendingRequestIds().contains(id));

    mediator.fulfillConsent(id, ConsentMediator::UserResponse::AllowOnce);

    QCOMPARE(spy.count(), 1);
    const auto args = spy.first();
    QCOMPARE(args.at(0).toString(), id);
    const auto decision = args.at(2).value<PolicyDecision>();
    QCOMPARE(decision.type, DecisionType::Allow);
    QVERIFY(!mediator.pendingRequestIds().contains(id));
}

void PolicyIntegrationTest::consentMediatorDenyAlwaysPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));

    ConsentMediator mediator;
    mediator.setPermissionStore(&store);

    ConsentMediator::ConsentRequest req;
    req.appId        = QStringLiteral("com.example.App");
    req.capability   = Capability::ShareInvoke;
    req.resourceType = QStringLiteral("share");
    req.resourceId   = QStringLiteral("all");
    req.timeoutMs    = 5000;

    const QString id = mediator.requestConsent(req);
    mediator.fulfillConsent(id, ConsentMediator::UserResponse::DenyAlways);

    // Verify stored as DenyAlways.
    const StoredPermission stored = store.findPermission(
        QStringLiteral("com.example.App"),
        Capability::ShareInvoke,
        QStringLiteral("share"), QStringLiteral("all"));
    QVERIFY(stored.valid);
    QCOMPARE(stored.decision, DecisionType::DenyAlways);
}

void PolicyIntegrationTest::consentMediatorAllowAlwaysPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));

    ConsentMediator mediator;
    mediator.setPermissionStore(&store);

    ConsentMediator::ConsentRequest req;
    req.appId        = QStringLiteral("com.example.App");
    req.capability   = Capability::SearchResolveClipboardResult;
    req.resourceType = QStringLiteral("clipboard");
    req.resourceId   = QStringLiteral("all");
    req.timeoutMs    = 5000;

    const QString id = mediator.requestConsent(req);
    mediator.fulfillConsent(id, ConsentMediator::UserResponse::AllowAlways);

    const StoredPermission stored = store.findPermission(
        QStringLiteral("com.example.App"),
        Capability::SearchResolveClipboardResult,
        QStringLiteral("clipboard"), QStringLiteral("all"));
    QVERIFY(stored.valid);
    QCOMPARE(stored.decision, DecisionType::AllowAlways);
}

void PolicyIntegrationTest::consentMediatorTimeoutDenies()
{
    ConsentMediator mediator;

    QSignalSpy spy(&mediator, &ConsentMediator::consentResolved);

    ConsentMediator::ConsentRequest req;
    req.appId      = QStringLiteral("com.example.App");
    req.capability = Capability::ShareInvoke;
    req.timeoutMs  = 50; // very short for test

    const QString id = mediator.requestConsent(req);

    // Wait for timeout.
    QVERIFY(spy.wait(500));
    QCOMPARE(spy.count(), 1);

    const auto args     = spy.first();
    const auto response = args.at(1).value<ConsentMediator::UserResponse>();
    const auto decision = args.at(2).value<PolicyDecision>();

    QCOMPARE(response, ConsentMediator::UserResponse::Timeout);
    QCOMPARE(decision.type, DecisionType::Deny);
    QVERIFY(!mediator.pendingRequestIds().contains(id));
}

void PolicyIntegrationTest::consentMediatorCancelDoesNotPersist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PermissionStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("perm.db"))));

    ConsentMediator mediator;
    mediator.setPermissionStore(&store);

    ConsentMediator::ConsentRequest req;
    req.appId      = QStringLiteral("com.example.App");
    req.capability = Capability::ShareInvoke;
    req.timeoutMs  = 5000;

    const QString id = mediator.requestConsent(req);
    mediator.cancelConsent(id);

    // No permission should be persisted.
    const StoredPermission stored = store.findPermission(
        QStringLiteral("com.example.App"), Capability::ShareInvoke);
    QVERIFY(!stored.valid);
}

void PolicyIntegrationTest::consentMediatorFulfillUnknownRequestIsNoOp()
{
    ConsentMediator mediator;
    QSignalSpy spy(&mediator, &ConsentMediator::consentResolved);

    // Fulfill a non-existent request — should not crash or emit.
    mediator.fulfillConsent(QStringLiteral("nonexistent-id"),
                            ConsentMediator::UserResponse::AllowOnce);

    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(PolicyIntegrationTest)
#include "policy_integration_test.moc"
