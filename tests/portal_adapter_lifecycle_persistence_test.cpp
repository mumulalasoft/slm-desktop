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
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDBusConnection>
#include <QDBusMessage>

using namespace Slm::Permissions;
using namespace Slm::PortalAdapter;

class PortalAdapterLifecyclePersistenceTest : public QObject
{
    Q_OBJECT

private slots:
    void requestLifecycle_success_and_closeNoop();
    void requestLifecycle_cancelOnClose();
    void sessionLifecycle_activate_close_revoke();
    void requestAndSessionManagers_removeFinishedObjects();
    void requestAndSessionIds_areUniqueWithinSameManager();
    void sessionOperation_rejectsCapabilityMismatch();
    void sessionOperation_screencastPayloadAndCloseSemantics();
    void sessionOperation_screencastStart_usesProvidedStreamDescriptor();
    void sessionOperation_inputCapture_payloadAndReleaseSemantics();
    void permissionStoreAdapter_roundtripAndPolicyReadback();
};

void PortalAdapterLifecyclePersistenceTest::requestLifecycle_success_and_closeNoop()
{
    CallerIdentity caller;
    caller.appId = QStringLiteral("org.example.TestApp");

    AccessContext context;
    context.capability = Capability::ShareInvoke;

    PortalRequestObject request(QStringLiteral("r1"),
                                QStringLiteral("/org/desktop/portal/request/org_example_TestApp/r1"),
                                caller,
                                Capability::ShareInvoke,
                                context);

    QSignalSpy responseSpy(&request, &PortalRequestObject::Response);
    QSignalSpy finishedSpy(&request, &PortalRequestObject::finished);

    request.setAwaitingUser();
    QCOMPARE(request.stateString(), QStringLiteral("AwaitingUser"));

    request.respondSuccess({{QStringLiteral("ok"), true}});
    QCOMPARE(request.stateString(), QStringLiteral("Allowed"));
    QVERIFY(request.isCompleted());
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    // Second completion attempt must be ignored.
    request.Close();
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
}

void PortalAdapterLifecyclePersistenceTest::requestLifecycle_cancelOnClose()
{
    CallerIdentity caller;
    caller.appId = QStringLiteral("org.example.TestApp");

    AccessContext context;
    context.capability = Capability::ShareInvoke;

    PortalRequestObject request(QStringLiteral("r2"),
                                QStringLiteral("/org/desktop/portal/request/org_example_TestApp/r2"),
                                caller,
                                Capability::ShareInvoke,
                                context);

    QSignalSpy responseSpy(&request, &PortalRequestObject::Response);
    QSignalSpy finishedSpy(&request, &PortalRequestObject::finished);

    request.Close();
    QCOMPARE(request.stateString(), QStringLiteral("Cancelled"));
    QVERIFY(request.isCompleted());
    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    const QList<QVariant> args = responseSpy.takeFirst();
    QCOMPARE(args.size(), 2);
    QCOMPARE(args.at(0).toUInt(), 2u);
    const QVariantMap payload = args.at(1).toMap();
    QCOMPARE(payload.value(QStringLiteral("error")).toString(), QStringLiteral("CancelledByUser"));
}

void PortalAdapterLifecyclePersistenceTest::sessionLifecycle_activate_close_revoke()
{
    CallerIdentity caller;
    caller.appId = QStringLiteral("org.example.TestApp");

    PortalSessionObject closable(QStringLiteral("s1"),
                                 QStringLiteral("/org/desktop/portal/session/org_example_TestApp/s1"),
                                 caller,
                                 Capability::ScreencastCreateSession,
                                 true);
    QSignalSpy closedSpy(&closable, &PortalSessionObject::Closed);
    QSignalSpy finishedSpy(&closable, &PortalSessionObject::sessionFinished);
    closable.activate();
    QVERIFY(closable.isActive());
    closable.Close();
    QCOMPARE(closable.stateString(), QStringLiteral("Closed"));
    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    PortalSessionObject revocable(QStringLiteral("s2"),
                                  QStringLiteral("/org/desktop/portal/session/org_example_TestApp/s2"),
                                  caller,
                                  Capability::ScreencastCreateSession,
                                  true);
    QSignalSpy revokedClosedSpy(&revocable, &PortalSessionObject::Closed);
    revocable.activate();
    revocable.revoke(QStringLiteral("policy-revoked"));
    QCOMPARE(revocable.stateString(), QStringLiteral("Revoked"));
    QCOMPARE(revokedClosedSpy.count(), 1);
    const QVariantMap details = revokedClosedSpy.takeFirst().at(0).toMap();
    QCOMPARE(details.value(QStringLiteral("revoked")).toBool(), true);
}

void PortalAdapterLifecyclePersistenceTest::requestAndSessionManagers_removeFinishedObjects()
{
    CallerIdentity caller;
    caller.appId = QStringLiteral("org.example.TestApp");
    AccessContext context;
    context.capability = Capability::ShareInvoke;

    PortalRequestManager requestManager;
    PortalRequestObject *request = nullptr;
    const QString requestPath =
        requestManager.createRequest(caller, Capability::ShareInvoke, context, &request);
    QVERIFY(!requestPath.isEmpty());
    QVERIFY(request != nullptr);
    QVERIFY(requestManager.request(requestPath) != nullptr);
    request->respondDenied(QStringLiteral("denied-for-test"));
    QCoreApplication::processEvents();
    QVERIFY(requestManager.request(requestPath) == nullptr);

    PortalSessionManager sessionManager;
    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        sessionManager.createSession(caller, Capability::ScreencastCreateSession, true, QString(), &session);
    QVERIFY(!sessionPath.isEmpty());
    QVERIFY(session != nullptr);
    QVERIFY(sessionManager.session(sessionPath) != nullptr);
    QVERIFY(sessionManager.closeSession(sessionPath));
    QCoreApplication::processEvents();
    QVERIFY(sessionManager.session(sessionPath) == nullptr);
}

void PortalAdapterLifecyclePersistenceTest::requestAndSessionIds_areUniqueWithinSameManager()
{
    CallerIdentity caller;
    caller.appId = QStringLiteral("org.example.TestApp");
    AccessContext context;
    context.capability = Capability::ShareInvoke;

    PortalRequestManager requestManager;
    PortalRequestObject *r1 = nullptr;
    PortalRequestObject *r2 = nullptr;
    const QString rp1 =
        requestManager.createRequest(caller, Capability::ShareInvoke, context, &r1);
    const QString rp2 =
        requestManager.createRequest(caller, Capability::ShareInvoke, context, &r2);
    QVERIFY(r1 != nullptr);
    QVERIFY(r2 != nullptr);
    QVERIFY(!rp1.isEmpty());
    QVERIFY(!rp2.isEmpty());
    QVERIFY(rp1 != rp2);

    PortalSessionManager sessionManager;
    PortalSessionObject *s1 = nullptr;
    PortalSessionObject *s2 = nullptr;
    const QString sp1 =
        sessionManager.createSession(caller, Capability::ScreencastCreateSession, true, QString(), &s1);
    const QString sp2 =
        sessionManager.createSession(caller, Capability::ScreencastCreateSession, true, QString(), &s2);
    QVERIFY(s1 != nullptr);
    QVERIFY(s2 != nullptr);
    QVERIFY(!sp1.isEmpty());
    QVERIFY(!sp2.isEmpty());
    QVERIFY(sp1 != sp2);
}

void PortalAdapterLifecyclePersistenceTest::sessionOperation_rejectsCapabilityMismatch()
{
    TrustResolver resolver;
    PermissionStore store;
    PolicyEngine engine;
    engine.setStore(&store);
    PermissionBroker broker;
    broker.setStore(&store);
    broker.setPolicyEngine(&engine);
    PortalCapabilityMapper mapper;
    PortalPermissionStoreAdapter storeAdapter;
    storeAdapter.setStore(&store);
    PortalRequestManager requestManager;
    PortalSessionManager sessionManager;

    PortalAccessMediator mediator;
    mediator.setTrustResolver(&resolver);
    mediator.setPermissionBroker(&broker);
    mediator.setCapabilityMapper(&mapper);
    mediator.setPermissionStoreAdapter(&storeAdapter);
    mediator.setRequestManager(&requestManager);
    mediator.setSessionManager(&sessionManager);

    CallerIdentity owner;
    owner.appId = QStringLiteral("org.example.TestApp");
    owner.trustLevel = TrustLevel::ThirdPartyApplication;
    owner.isOfficialComponent = false;

    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        sessionManager.createSession(owner, Capability::GlobalShortcutsCreateSession, true, QString(), &session);
    QVERIFY(session != nullptr);
    session->activate();

    QVariantMap params;
    params.insert(QStringLiteral("initiatedByUserGesture"), true);
    params.insert(QStringLiteral("resourceType"), QStringLiteral("session-op"));
    params.insert(QStringLiteral("resourceId"), QStringLiteral("x"));

    const QVariantMap result = mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStart"),
        QDBusMessage(),
        sessionPath,
        params,
        true);

    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("InvalidRequest"));
    QCOMPARE(result.value(QStringLiteral("reason")).toString(),
             QStringLiteral("session-capability-mismatch"));
}

void PortalAdapterLifecyclePersistenceTest::sessionOperation_screencastPayloadAndCloseSemantics()
{
    const QString busName = QStringLiteral("org.slm.desktop.fake");
    TrustResolver resolver;
    resolver.setPrivilegedServiceNames({busName});

    PermissionStore store;
    PolicyEngine engine;
    engine.setStore(&store);
    PermissionBroker broker;
    broker.setStore(&store);
    broker.setPolicyEngine(&engine);
    PortalCapabilityMapper mapper;
    PortalPermissionStoreAdapter storeAdapter;
    storeAdapter.setStore(&store);
    PortalRequestManager requestManager;
    PortalSessionManager sessionManager;

    PortalAccessMediator mediator;
    mediator.setTrustResolver(&resolver);
    mediator.setPermissionBroker(&broker);
    mediator.setCapabilityMapper(&mapper);
    mediator.setPermissionStoreAdapter(&storeAdapter);
    mediator.setRequestManager(&requestManager);
    mediator.setSessionManager(&sessionManager);

    CallerIdentity owner;
    owner.appId = QStringLiteral("slm-portald-test");
    owner.busName = busName;
    owner.trustLevel = TrustLevel::PrivilegedDesktopService;
    owner.isOfficialComponent = false;

    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        sessionManager.createSession(owner, Capability::ScreencastCreateSession, true, QString(), &session);
    QVERIFY(session != nullptr);
    session->activate();

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        busName,
        QStringLiteral("/org/slm/Test"),
        QStringLiteral("org.slm.Test"),
        QStringLiteral("Run"));

    QVariantMap selectParams;
    selectParams.insert(QStringLiteral("initiatedByUserGesture"), true);
    selectParams.insert(QStringLiteral("sources"), QVariantList{QStringLiteral("window")});
    selectParams.insert(QStringLiteral("multiple"), true);
    const QVariantMap selectOut = mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastSelectSources"),
        msg,
        sessionPath,
        selectParams,
        true);
    QVERIFY(selectOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap selectResults = selectOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(selectResults.value(QStringLiteral("sources_selected")).toBool(), true);
    QVERIFY(selectResults.contains(QStringLiteral("selected_sources")));

    QVariantMap startParams;
    startParams.insert(QStringLiteral("initiatedByUserGesture"), true);
    const QVariantMap startOut = mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStart"),
        msg,
        sessionPath,
        startParams,
        true);
    QVERIFY(startOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap startResults = startOut.value(QStringLiteral("results")).toMap();
    const QVariantList streams = startResults.value(QStringLiteral("streams")).toList();
    QVERIFY(!streams.isEmpty());
    const QVariantMap firstStream = streams.constFirst().toMap();
    QVERIFY(firstStream.contains(QStringLiteral("stream_id")));
    QVERIFY(firstStream.contains(QStringLiteral("node_id")));
    QVERIFY(firstStream.contains(QStringLiteral("source_type")));

    QVariantMap stopKeepParams;
    stopKeepParams.insert(QStringLiteral("closeSession"), false);
    const QVariantMap stopKeepOut = mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStop"),
        msg,
        sessionPath,
        stopKeepParams,
        false);
    QVERIFY(stopKeepOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap stopKeepResults = stopKeepOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(stopKeepResults.value(QStringLiteral("stopped")).toBool(), true);
    QCOMPARE(stopKeepResults.value(QStringLiteral("session_closed")).toBool(), false);
    QVERIFY(sessionManager.session(sessionPath) != nullptr);

    QVariantMap stopCloseParams;
    stopCloseParams.insert(QStringLiteral("closeSession"), true);
    const QVariantMap stopCloseOut = mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStop"),
        msg,
        sessionPath,
        stopCloseParams,
        false);
    QVERIFY(stopCloseOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap stopCloseResults = stopCloseOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(stopCloseResults.value(QStringLiteral("stopped")).toBool(), true);
    QCOMPARE(stopCloseResults.value(QStringLiteral("session_closed")).toBool(), true);
    QCoreApplication::processEvents();
    QVERIFY(sessionManager.session(sessionPath) == nullptr);
}

void PortalAdapterLifecyclePersistenceTest::sessionOperation_screencastStart_usesProvidedStreamDescriptor()
{
    const QString busName = QStringLiteral("org.slm.desktop.fake");
    TrustResolver resolver;
    resolver.setPrivilegedServiceNames({busName});

    PermissionStore store;
    PolicyEngine engine;
    engine.setStore(&store);
    PermissionBroker broker;
    broker.setStore(&store);
    broker.setPolicyEngine(&engine);
    PortalCapabilityMapper mapper;
    PortalPermissionStoreAdapter storeAdapter;
    storeAdapter.setStore(&store);
    PortalRequestManager requestManager;
    PortalSessionManager sessionManager;

    PortalAccessMediator mediator;
    mediator.setTrustResolver(&resolver);
    mediator.setPermissionBroker(&broker);
    mediator.setCapabilityMapper(&mapper);
    mediator.setPermissionStoreAdapter(&storeAdapter);
    mediator.setRequestManager(&requestManager);
    mediator.setSessionManager(&sessionManager);

    CallerIdentity owner;
    owner.appId = QStringLiteral("slm-portald-test");
    owner.busName = busName;
    owner.trustLevel = TrustLevel::PrivilegedDesktopService;
    owner.isOfficialComponent = false;

    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        sessionManager.createSession(owner, Capability::ScreencastCreateSession, true, QString(), &session);
    QVERIFY(session != nullptr);
    session->activate();

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        busName,
        QStringLiteral("/org/slm/Test"),
        QStringLiteral("org.slm.Test"),
        QStringLiteral("Run"));

    QVariantMap startParams;
    startParams.insert(QStringLiteral("initiatedByUserGesture"), true);
    startParams.insert(QStringLiteral("node_id"), 4242);
    startParams.insert(QStringLiteral("stream_id"), 99);
    startParams.insert(QStringLiteral("source_type"), QStringLiteral("window"));
    startParams.insert(QStringLiteral("cursor_mode"), QStringLiteral("metadata"));

    const QVariantMap startOut = mediator.handlePortalSessionOperation(
        QStringLiteral("ScreencastStart"),
        msg,
        sessionPath,
        startParams,
        true);
    QVERIFY(startOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap startResults = startOut.value(QStringLiteral("results")).toMap();
    const QVariantList streams = startResults.value(QStringLiteral("streams")).toList();
    QVERIFY(!streams.isEmpty());
    const QVariantMap stream = streams.constFirst().toMap();
    QCOMPARE(stream.value(QStringLiteral("node_id")).toInt(), 4242);
    QCOMPARE(stream.value(QStringLiteral("stream_id")).toInt(), 99);
    QCOMPARE(stream.value(QStringLiteral("source_type")).toString(), QStringLiteral("window"));
    QCOMPARE(stream.value(QStringLiteral("cursor_mode")).toString(), QStringLiteral("metadata"));
}

void PortalAdapterLifecyclePersistenceTest::sessionOperation_inputCapture_payloadAndReleaseSemantics()
{
    const QString busName = QStringLiteral("org.slm.desktop.fake");
    TrustResolver resolver;
    resolver.setPrivilegedServiceNames({busName});

    PermissionStore store;
    PolicyEngine engine;
    engine.setStore(&store);
    PermissionBroker broker;
    broker.setStore(&store);
    broker.setPolicyEngine(&engine);
    PortalCapabilityMapper mapper;
    PortalPermissionStoreAdapter storeAdapter;
    storeAdapter.setStore(&store);
    PortalRequestManager requestManager;
    PortalSessionManager sessionManager;

    PortalAccessMediator mediator;
    mediator.setTrustResolver(&resolver);
    mediator.setPermissionBroker(&broker);
    mediator.setCapabilityMapper(&mapper);
    mediator.setPermissionStoreAdapter(&storeAdapter);
    mediator.setRequestManager(&requestManager);
    mediator.setSessionManager(&sessionManager);

    CallerIdentity owner;
    owner.appId = QStringLiteral("slm-portald-test");
    owner.busName = busName;
    owner.trustLevel = TrustLevel::PrivilegedDesktopService;
    owner.isOfficialComponent = false;

    PortalSessionObject *session = nullptr;
    const QString sessionPath =
        sessionManager.createSession(owner, Capability::InputCaptureCreateSession, true, QString(), &session);
    QVERIFY(session != nullptr);
    session->activate();

    const QDBusMessage msg = QDBusMessage::createMethodCall(
        busName,
        QStringLiteral("/org/slm/Test"),
        QStringLiteral("org.slm.Test"),
        QStringLiteral("Run"));

    const QVariantList barriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("left-edge")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 0},
            {QStringLiteral("y2"), 1080},
        },
    };
    QVariantMap barrierParams;
    barrierParams.insert(QStringLiteral("initiatedByUserGesture"), true);
    barrierParams.insert(QStringLiteral("barriers"), barriers);
    const QVariantMap barrierOut = mediator.handlePortalSessionOperation(
        QStringLiteral("InputCaptureSetPointerBarriers"),
        msg,
        sessionPath,
        barrierParams,
        true);
    QVERIFY(barrierOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap barrierResults = barrierOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(barrierResults.value(QStringLiteral("barriers_set")).toBool(), true);
    QCOMPARE(barrierResults.value(QStringLiteral("pointer_barriers")).toList().size(), 1);

    const QVariantMap enableOut = mediator.handlePortalSessionOperation(
        QStringLiteral("InputCaptureEnable"),
        msg,
        sessionPath,
        QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}},
        true);
    QVERIFY(enableOut.value(QStringLiteral("ok")).toBool());
    QCOMPARE(enableOut.value(QStringLiteral("results")).toMap().value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(session->metadata().value(QStringLiteral("inputcapture.enabled")).toBool(), true);

    const QVariantMap disableOut = mediator.handlePortalSessionOperation(
        QStringLiteral("InputCaptureDisable"),
        msg,
        sessionPath,
        QVariantMap{},
        true);
    QVERIFY(disableOut.value(QStringLiteral("ok")).toBool());
    QCOMPARE(disableOut.value(QStringLiteral("results")).toMap().value(QStringLiteral("disabled")).toBool(), true);
    QCOMPARE(session->metadata().value(QStringLiteral("inputcapture.enabled")).toBool(), false);

    const QVariantMap releaseOut = mediator.handlePortalSessionOperation(
        QStringLiteral("InputCaptureRelease"),
        msg,
        sessionPath,
        QVariantMap{{QStringLiteral("closeSession"), true}},
        false);
    QVERIFY(releaseOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap releaseResults = releaseOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(releaseResults.value(QStringLiteral("released")).toBool(), true);
    QCOMPARE(releaseResults.value(QStringLiteral("session_closed")).toBool(), true);
    QCoreApplication::processEvents();
    QVERIFY(sessionManager.session(sessionPath) == nullptr);
}

void PortalAdapterLifecyclePersistenceTest::permissionStoreAdapter_roundtripAndPolicyReadback()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("permissions.db"));

    PermissionStore store;
    QVERIFY(store.open(dbPath));

    PortalPermissionStoreAdapter adapter;
    adapter.setStore(&store);

    const QString appId = QStringLiteral("org.example.TestApp");
    const Capability capability = Capability::ShareInvoke;
    const QString resourceType = QStringLiteral("share-target");
    const QString resourceId = QStringLiteral("all");

    QVERIFY(adapter.saveDecision(appId,
                                 capability,
                                 resourceType,
                                 resourceId,
                                 DecisionType::AllowAlways,
                                 QStringLiteral("persistent")));

    const PortalStoredDecision saved = adapter.loadDecision(appId, capability, resourceType, resourceId);
    QVERIFY(saved.found);
    QCOMPARE(saved.decision, DecisionType::AllowAlways);
    QCOMPARE(saved.scope, QStringLiteral("persistent"));

    const QVariantList rows = adapter.listDecisions(appId);
    QVERIFY(!rows.isEmpty());

    PolicyEngine engine;
    engine.setStore(&store);
    CallerIdentity caller;
    caller.appId = appId;
    caller.trustLevel = TrustLevel::ThirdPartyApplication;

    AccessContext context;
    context.capability = capability;
    context.resourceType = resourceType;
    context.resourceId = resourceId;
    context.sensitivityLevel = engine.sensitivityForCapability(capability);

    const PolicyDecision decisionWithStoredAllow = engine.evaluate(caller, context);
    QCOMPARE(decisionWithStoredAllow.type, DecisionType::Allow);
    QCOMPARE(decisionWithStoredAllow.reason, QStringLiteral("stored-policy"));

    QVERIFY(adapter.saveDecision(appId,
                                 capability,
                                 resourceType,
                                 resourceId,
                                 DecisionType::DenyAlways,
                                 QStringLiteral("persistent")));
    const PolicyDecision decisionWithStoredDeny = engine.evaluate(caller, context);
    QCOMPARE(decisionWithStoredDeny.type, DecisionType::Deny);
    QCOMPARE(decisionWithStoredDeny.reason, QStringLiteral("stored-policy"));
}

QTEST_MAIN(PortalAdapterLifecyclePersistenceTest)
#include "portal_adapter_lifecycle_persistence_test.moc"
