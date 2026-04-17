#include <QtTest>

#include "src/core/permissions/AccessContext.h"
#include "src/core/permissions/Capability.h"
#include "src/core/permissions/CallerIdentity.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/TrustResolver.h"

#include "src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.h"
#include "src/daemon/portald/portal-adapter/PortalRequestManager.h"
#include "src/daemon/portald/portal-adapter/PortalRequestObject.h"
#include "src/daemon/portald/portal-adapter/PortalSessionManager.h"
#include "src/daemon/portald/portal-adapter/PortalSessionObject.h"
#include "src/daemon/portald/portal-adapter/PortalAccessMediator.h"
#include "src/daemon/portald/portal-adapter/PortalCapabilityMapper.h"

#include <QSignalSpy>
#include <QDBusMessage>

using namespace Slm::Permissions;
using namespace Slm::PortalAdapter;

// ── Fixture helpers ────────────────────────────────────────────────────────────

static CallerIdentity makePrivilegedCaller(const QString &busName)
{
    CallerIdentity c;
    c.appId    = QStringLiteral("slm-portald-test");
    c.busName  = busName;
    c.trustLevel = TrustLevel::PrivilegedDesktopService;
    c.isOfficialComponent = false;
    return c;
}

static CallerIdentity makeThirdPartyCaller(const QString &appId)
{
    CallerIdentity c;
    c.appId    = appId;
    c.busName  = QStringLiteral("com.example.Test");
    c.trustLevel = TrustLevel::ThirdPartyApplication;
    c.isOfficialComponent = false;
    return c;
}

struct MediatorFixture
{
    const QString busName = QStringLiteral("org.slm.desktop.fake");

    TrustResolver              resolver;
    PermissionStore            store;
    PolicyEngine               engine;
    PermissionBroker           broker;
    PortalCapabilityMapper     mapper;
    PortalPermissionStoreAdapter storeAdapter;
    PortalRequestManager       requestManager;
    PortalSessionManager       sessionManager;
    PortalAccessMediator       mediator;

    MediatorFixture()
    {
        resolver.setPrivilegedServiceNames({busName});
        engine.setStore(&store);
        broker.setStore(&store);
        broker.setPolicyEngine(&engine);
        storeAdapter.setStore(&store);
        mediator.setTrustResolver(&resolver);
        mediator.setPermissionBroker(&broker);
        mediator.setCapabilityMapper(&mapper);
        mediator.setPermissionStoreAdapter(&storeAdapter);
        mediator.setRequestManager(&requestManager);
        mediator.setSessionManager(&sessionManager);
    }

    CallerIdentity privilegedCaller() const { return makePrivilegedCaller(busName); }
};

// ── Test class ─────────────────────────────────────────────────────────────────

class PortalAdapterRaceCancellationTest : public QObject
{
    Q_OBJECT

private slots:
    // Double-close/double-cancel no-op safety
    void request_doubleClose_isNoOp();
    void request_closeAfterSuccess_isNoOp();
    void request_closeAfterDenied_isNoOp();
    void session_doubleClose_isNoOp();
    void session_doubleRevoke_isNoOp();
    void session_revokeBeforeActivate_isNoOp();

    // Rapid create/destroy stress
    void requestManager_rapidCreateAndCancelAll_cleansUp();
    void sessionManager_rapidCreateAndCloseAll_cleansUp();

    // Cancellation ordering
    void request_respondDeniedThenClose_onlySingleResponse();
    void request_respondCancelledThenRespondSuccess_onlySingleResponse();

    // Session operation on terminal state
    void sessionOperation_onClosedSession_returnsError();
    void sessionOperation_onRevokedSession_returnsError();

    // Revocation while operation in flight
    void session_revokeWhileScreencastActive_closesAndCleansUp();

    // Multi-session isolation
    void multiSession_revokeOneDoesNotAffectOther();

    // Manager signals
    void requestManager_emitsCreatedAndRemoved();
    void sessionManager_emitsCreatedAndRemoved();
};

// ── Double-close / double-cancel ───────────────────────────────────────────────

void PortalAdapterRaceCancellationTest::request_doubleClose_isNoOp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));
    AccessContext ctx;
    ctx.capability = Capability::ShareInvoke;

    PortalRequestObject req(QStringLiteral("r-dc"),
                            QStringLiteral("/org/desktop/portal/request/org_example_App/r-dc"),
                            caller,
                            Capability::ShareInvoke,
                            ctx);

    QSignalSpy responseSpy(&req, &PortalRequestObject::Response);
    QSignalSpy finishedSpy(&req, &PortalRequestObject::finished);

    req.Close();
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(req.isCompleted());

    // Second Close must be a no-op.
    req.Close();
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
}

void PortalAdapterRaceCancellationTest::request_closeAfterSuccess_isNoOp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));
    AccessContext ctx;
    ctx.capability = Capability::ShareInvoke;

    PortalRequestObject req(QStringLiteral("r-cas"),
                            QStringLiteral("/org/desktop/portal/request/org_example_App/r-cas"),
                            caller,
                            Capability::ShareInvoke,
                            ctx);

    QSignalSpy responseSpy(&req, &PortalRequestObject::Response);
    QSignalSpy finishedSpy(&req, &PortalRequestObject::finished);

    req.respondSuccess({{QStringLiteral("ok"), true}});
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(req.stateString(), QStringLiteral("Allowed"));

    // Close after completion must be a no-op.
    req.Close();
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(req.stateString(), QStringLiteral("Allowed"));
}

void PortalAdapterRaceCancellationTest::request_closeAfterDenied_isNoOp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));
    AccessContext ctx;
    ctx.capability = Capability::ShareInvoke;

    PortalRequestObject req(QStringLiteral("r-cad"),
                            QStringLiteral("/org/desktop/portal/request/org_example_App/r-cad"),
                            caller,
                            Capability::ShareInvoke,
                            ctx);

    QSignalSpy responseSpy(&req, &PortalRequestObject::Response);
    QSignalSpy finishedSpy(&req, &PortalRequestObject::finished);

    req.respondDenied(QStringLiteral("policy-deny"));
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(req.stateString(), QStringLiteral("Denied"));

    req.Close();
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(req.stateString(), QStringLiteral("Denied"));
}

void PortalAdapterRaceCancellationTest::session_doubleClose_isNoOp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));

    PortalSessionObject session(QStringLiteral("s-dc"),
                                QStringLiteral("/org/desktop/portal/session/org_example_App/s-dc"),
                                caller,
                                Capability::ScreencastCreateSession,
                                true);

    QSignalSpy closedSpy(&session, &PortalSessionObject::Closed);
    QSignalSpy finishedSpy(&session, &PortalSessionObject::sessionFinished);

    session.activate();
    QVERIFY(session.isActive());

    session.Close();
    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(session.stateString(), QStringLiteral("Closed"));

    // Second Close must be a no-op.
    session.Close();
    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(session.stateString(), QStringLiteral("Closed"));
}

void PortalAdapterRaceCancellationTest::session_doubleRevoke_isNoOp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));

    PortalSessionObject session(QStringLiteral("s-dr"),
                                QStringLiteral("/org/desktop/portal/session/org_example_App/s-dr"),
                                caller,
                                Capability::ScreencastCreateSession,
                                true);

    QSignalSpy closedSpy(&session, &PortalSessionObject::Closed);

    session.activate();
    session.revoke(QStringLiteral("policy-changed"));
    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(session.stateString(), QStringLiteral("Revoked"));

    // Second revoke must be a no-op.
    session.revoke(QStringLiteral("policy-changed-again"));
    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(session.stateString(), QStringLiteral("Revoked"));
}

void PortalAdapterRaceCancellationTest::session_revokeBeforeActivate_isNoOp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));

    PortalSessionObject session(QStringLiteral("s-rba"),
                                QStringLiteral("/org/desktop/portal/session/org_example_App/s-rba"),
                                caller,
                                Capability::ScreencastCreateSession,
                                true);

    QSignalSpy closedSpy(&session, &PortalSessionObject::Closed);

    // Revoke without activate — must not emit Closed and must not crash.
    session.revoke(QStringLiteral("premature-revoke"));
    QCOMPARE(closedSpy.count(), 0);
    // State should remain Created (not transitioned to Revoked) because
    // revoke on a non-active session is undefined; the important contract
    // is no crash and no spurious Closed signal.
    QVERIFY(!session.isActive());
}

// ── Rapid create / destroy stress ─────────────────────────────────────────────

void PortalAdapterRaceCancellationTest::requestManager_rapidCreateAndCancelAll_cleansUp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.StressApp"));
    AccessContext ctx;
    ctx.capability = Capability::ShareInvoke;

    PortalRequestManager manager;
    constexpr int N = 20;
    QStringList paths;
    paths.reserve(N);

    for (int i = 0; i < N; ++i) {
        PortalRequestObject *req = nullptr;
        const QString path = manager.createRequest(caller, Capability::ShareInvoke, ctx, &req);
        QVERIFY(!path.isEmpty());
        QVERIFY(req != nullptr);
        paths.append(path);
    }

    // All N requests exist.
    for (const QString &p : std::as_const(paths))
        QVERIFY(manager.request(p) != nullptr);

    // Cancel them all.
    for (const QString &p : std::as_const(paths)) {
        PortalRequestObject *req = manager.request(p);
        QVERIFY(req != nullptr);
        req->Close();
    }
    QCoreApplication::processEvents();

    // Manager must be empty.
    for (const QString &p : std::as_const(paths))
        QVERIFY(manager.request(p) == nullptr);
}

void PortalAdapterRaceCancellationTest::sessionManager_rapidCreateAndCloseAll_cleansUp()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.StressApp"));

    PortalSessionManager manager;
    constexpr int N = 20;
    QStringList paths;
    paths.reserve(N);

    for (int i = 0; i < N; ++i) {
        PortalSessionObject *session = nullptr;
        const QString path = manager.createSession(
            caller, Capability::ScreencastCreateSession, true, QString(), &session);
        QVERIFY(!path.isEmpty());
        QVERIFY(session != nullptr);
        session->activate();
        paths.append(path);
    }

    // Close them all.
    for (const QString &p : std::as_const(paths))
        QVERIFY(manager.closeSession(p));

    QCoreApplication::processEvents();

    for (const QString &p : std::as_const(paths))
        QVERIFY(manager.session(p) == nullptr);
}

// ── Cancellation ordering ──────────────────────────────────────────────────────

void PortalAdapterRaceCancellationTest::request_respondDeniedThenClose_onlySingleResponse()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));
    AccessContext ctx;
    ctx.capability = Capability::ShareInvoke;

    PortalRequestObject req(QStringLiteral("r-rdtc"),
                            QStringLiteral("/org/desktop/portal/request/org_example_App/r-rdtc"),
                            caller,
                            Capability::ShareInvoke,
                            ctx);

    QSignalSpy responseSpy(&req, &PortalRequestObject::Response);

    req.respondDenied(QStringLiteral("policy"));
    req.Close(); // must be ignored
    req.respondSuccess({{}}); // must be ignored

    QCOMPARE(responseSpy.count(), 1);
    const QList<QVariant> args = responseSpy.first();
    // response code 1 = Denied
    QCOMPARE(args.at(0).toUInt(), 1u);
}

void PortalAdapterRaceCancellationTest::request_respondCancelledThenRespondSuccess_onlySingleResponse()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));
    AccessContext ctx;
    ctx.capability = Capability::ShareInvoke;

    PortalRequestObject req(QStringLiteral("r-rctrs"),
                            QStringLiteral("/org/desktop/portal/request/org_example_App/r-rctrs"),
                            caller,
                            Capability::ShareInvoke,
                            ctx);

    QSignalSpy responseSpy(&req, &PortalRequestObject::Response);

    req.respondCancelled(QStringLiteral("user-dismiss"));
    req.respondSuccess({{QStringLiteral("ok"), true}}); // must be ignored

    QCOMPARE(responseSpy.count(), 1);
    const QList<QVariant> args = responseSpy.first();
    // response code 2 = Cancelled
    QCOMPARE(args.at(0).toUInt(), 2u);
}

// ── Session operation on terminal state ───────────────────────────────────────

void PortalAdapterRaceCancellationTest::sessionOperation_onClosedSession_returnsError()
{
    MediatorFixture f;
    const CallerIdentity owner = f.privilegedCaller();

    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        f.sessionManager.createSession(owner, Capability::ScreencastCreateSession, true, QString(), &session);
    QVERIFY(session != nullptr);
    session->activate();

    // Close the session.
    f.sessionManager.closeSession(sessionPath);
    QCoreApplication::processEvents();
    QVERIFY(f.sessionManager.session(sessionPath) == nullptr);

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        f.busName,
        QStringLiteral("/org/slm/Test"),
        QStringLiteral("org.slm.Test"),
        QStringLiteral("Run"));

    const QVariantMap result = f.mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStart"),
        msg,
        sessionPath,
        QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}},
        true);

    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(!result.value(QStringLiteral("error")).toString().isEmpty());
}

void PortalAdapterRaceCancellationTest::sessionOperation_onRevokedSession_returnsError()
{
    MediatorFixture f;
    const CallerIdentity owner = f.privilegedCaller();

    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        f.sessionManager.createSession(owner, Capability::ScreencastCreateSession, true, QString(), &session);
    QVERIFY(session != nullptr);
    session->activate();
    session->revoke(QStringLiteral("admin-revoke"));

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        f.busName,
        QStringLiteral("/org/slm/Test"),
        QStringLiteral("org.slm.Test"),
        QStringLiteral("Run"));

    const QVariantMap result = f.mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStart"),
        msg,
        sessionPath,
        QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}},
        true);

    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(!result.value(QStringLiteral("error")).toString().isEmpty());
}

// ── Revocation while operation in flight ──────────────────────────────────────

void PortalAdapterRaceCancellationTest::session_revokeWhileScreencastActive_closesAndCleansUp()
{
    MediatorFixture f;
    const CallerIdentity owner = f.privilegedCaller();

    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        f.sessionManager.createSession(owner, Capability::ScreencastCreateSession, true, QString(), &session);
    QVERIFY(session != nullptr);
    session->activate();

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        f.busName,
        QStringLiteral("/org/slm/Test"),
        QStringLiteral("org.slm.Test"),
        QStringLiteral("Run"));

    // Start a screencast.
    QVariantMap startParams;
    startParams.insert(QStringLiteral("initiatedByUserGesture"), true);
    const QVariantMap startOut = f.mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStart"), msg, sessionPath, startParams, true);
    QVERIFY(startOut.value(QStringLiteral("ok")).toBool());

    QSignalSpy closedSpy(session, &PortalSessionObject::Closed);
    QSignalSpy removedSpy(&f.sessionManager, &PortalSessionManager::sessionRemoved);

    // Revoke the session while screencast is running.
    session->revoke(QStringLiteral("policy-change-mid-stream"));

    QCOMPARE(session->stateString(), QStringLiteral("Revoked"));
    QCOMPARE(closedSpy.count(), 1);
    const QVariantMap details = closedSpy.first().at(0).toMap();
    QCOMPARE(details.value(QStringLiteral("revoked")).toBool(), true);

    // Manager cleanup happens asynchronously via finished signal.
    QCoreApplication::processEvents();
    QVERIFY(f.sessionManager.session(sessionPath) == nullptr);
    QCOMPARE(removedSpy.count(), 1);
}

// ── Multi-session isolation ────────────────────────────────────────────────────

void PortalAdapterRaceCancellationTest::multiSession_revokeOneDoesNotAffectOther()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));

    PortalSessionManager manager;

    PortalSessionObject *s1 = nullptr;
    PortalSessionObject *s2 = nullptr;
    const QString sp1 =
        manager.createSession(caller, Capability::ScreencastCreateSession, true, QString(), &s1);
    const QString sp2 =
        manager.createSession(caller, Capability::ScreencastCreateSession, true, QString(), &s2);

    QVERIFY(s1 != nullptr);
    QVERIFY(s2 != nullptr);
    QVERIFY(sp1 != sp2);

    s1->activate();
    s2->activate();

    QSignalSpy s2ClosedSpy(s2, &PortalSessionObject::Closed);

    // Revoke s1 only.
    s1->revoke(QStringLiteral("isolated-revoke"));
    QCOMPARE(s1->stateString(), QStringLiteral("Revoked"));

    // s2 must be untouched.
    QVERIFY(s2->isActive());
    QCOMPARE(s2ClosedSpy.count(), 0);
    QCOMPARE(s2->stateString(), QStringLiteral("Active"));

    QCoreApplication::processEvents();
    QVERIFY(manager.session(sp2) != nullptr);
}

// ── Manager signals ────────────────────────────────────────────────────────────

void PortalAdapterRaceCancellationTest::requestManager_emitsCreatedAndRemoved()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));
    AccessContext ctx;
    ctx.capability = Capability::ShareInvoke;

    PortalRequestManager manager;
    QSignalSpy createdSpy(&manager, &PortalRequestManager::requestCreated);
    QSignalSpy removedSpy(&manager, &PortalRequestManager::requestRemoved);

    PortalRequestObject *req = nullptr;
    const QString path = manager.createRequest(caller, Capability::ShareInvoke, ctx, &req);
    QVERIFY(!path.isEmpty());
    QCOMPARE(createdSpy.count(), 1);
    QCOMPARE(createdSpy.first().at(0).toString(), path);

    req->respondSuccess({});
    QCoreApplication::processEvents();

    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().at(0).toString(), path);
}

void PortalAdapterRaceCancellationTest::sessionManager_emitsCreatedAndRemoved()
{
    CallerIdentity caller = makeThirdPartyCaller(QStringLiteral("org.example.App"));

    PortalSessionManager manager;
    QSignalSpy createdSpy(&manager, &PortalSessionManager::sessionCreated);
    QSignalSpy removedSpy(&manager, &PortalSessionManager::sessionRemoved);

    PortalSessionObject *session = nullptr;
    const QString path =
        manager.createSession(caller, Capability::ScreencastCreateSession, true, QString(), &session);
    QVERIFY(!path.isEmpty());
    QCOMPARE(createdSpy.count(), 1);
    QCOMPARE(createdSpy.first().at(0).toString(), path);

    session->activate();
    manager.closeSession(path);
    QCoreApplication::processEvents();

    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().at(0).toString(), path);
}

QTEST_MAIN(PortalAdapterRaceCancellationTest)
#include "portal_adapter_race_cancellation_test.moc"
