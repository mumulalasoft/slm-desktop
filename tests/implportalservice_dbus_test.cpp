#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QTemporaryDir>

#include "../src/daemon/portald/implportalservice.h"
#include "../src/daemon/portald/portalmanager.h"
#include "../src/daemon/portald/portal-adapter/PortalBackendService.h"
#include "../src/core/permissions/PermissionStore.h"
#include "../src/core/permissions/PolicyEngine.h"
#include "../src/core/permissions/PermissionBroker.h"
#include "../src/core/permissions/TrustResolver.h"
#include "../src/core/permissions/Capability.h"
#include "../src/core/permissions/PolicyDecision.h"

namespace {
constexpr const char kService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kIface[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kIfaceFileChooser[] = "org.freedesktop.impl.portal.FileChooser";
constexpr const char kIfaceOpenUri[] = "org.freedesktop.impl.portal.OpenURI";
constexpr const char kIfaceScreenshot[] = "org.freedesktop.impl.portal.Screenshot";
constexpr const char kIfaceScreenCast[] = "org.freedesktop.impl.portal.ScreenCast";
constexpr const char kIfaceGlobalShortcuts[] = "org.freedesktop.impl.portal.GlobalShortcuts";
constexpr const char kIfaceInputCapture[] = "org.freedesktop.impl.portal.InputCapture";
constexpr const char kIfaceSettings[] = "org.freedesktop.impl.portal.Settings";
constexpr const char kIfaceNotification[] = "org.freedesktop.impl.portal.Notification";
constexpr const char kIfaceInhibit[] = "org.freedesktop.impl.portal.Inhibit";
constexpr const char kIfaceOpenWith[] = "org.freedesktop.impl.portal.OpenWith";
constexpr const char kIfaceDocuments[] = "org.freedesktop.impl.portal.Documents";
constexpr const char kIfaceTrash[] = "org.freedesktop.impl.portal.Trash";
constexpr const char kUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kUiPath[] = "/org/slm/Desktop/PortalUI";
constexpr const char kCaptureService[] = "org.slm.Desktop.Capture";
constexpr const char kCapturePath[] = "/org/slm/Desktop/Capture";
constexpr const char kScreencastService[] = "org.slm.Desktop.Screencast";
constexpr const char kScreencastPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kInputCaptureService[] = "org.slm.Desktop.InputCapture";
constexpr const char kInputCapturePath[] = "/org/slm/Desktop/InputCapture";
}

class FakePortalUiService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.PortalUI")

public:
    QVariantMap response{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("canceled")},
    };
    QVariantMap consentResponse{
        {QStringLiteral("decision"), QStringLiteral("deny")},
        {QStringLiteral("persist"), false},
        {QStringLiteral("scope"), QStringLiteral("session")},
        {QStringLiteral("reason"), QStringLiteral("consent-denied")},
    };

public slots:
    QVariantMap FileChooser(const QVariantMap &)
    {
        return response;
    }

    QVariantMap ConsentRequest(const QVariantMap &)
    {
        return consentResponse;
    }
};

class FakeCaptureService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Capture")

public:
    int getStreamsCalls = 0;
    int clearCalls = 0;
    int revokeCalls = 0;
    QString lastClearedSession;
    QString lastRevokedSession;
    QString lastRevokeReason;

public slots:
    QVariantMap GetScreencastStreams(const QString &sessionPath,
                                     const QString &,
                                     const QVariantMap &)
    {
        ++getStreamsCalls;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("results"),
             QVariantMap{
                 {QStringLiteral("streams"),
                  QVariantList{
                      QVariantMap{
                          {QStringLiteral("stream_id"), 314u},
                          {QStringLiteral("node_id"), 2718u},
                          {QStringLiteral("source_type"), QStringLiteral("window")},
                          {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
                          {QStringLiteral("session"), sessionPath},
                      },
                  }},
            }},
        };
    }

    QVariantMap ClearScreencastSession(const QString &sessionPath)
    {
        ++clearCalls;
        lastClearedSession = sessionPath;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("response"), 0u},
            {QStringLiteral("results"), QVariantMap{{QStringLiteral("session_handle"), sessionPath}}},
        };
    }

    QVariantMap RevokeScreencastSession(const QString &sessionPath, const QString &reason)
    {
        ++revokeCalls;
        lastRevokedSession = sessionPath;
        lastRevokeReason = reason;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("response"), 0u},
            {QStringLiteral("results"),
             QVariantMap{
                 {QStringLiteral("session_handle"), sessionPath},
                 {QStringLiteral("reason"), reason},
             }},
        };
    }
};

class FakeScreencastService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Screencast")

public:
    int getSessionStreamsCalls = 0;

public slots:
    QVariantMap GetSessionStreams(const QString &sessionPath)
    {
        ++getSessionStreamsCalls;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("results"),
             QVariantMap{
                 {QStringLiteral("session_handle"), sessionPath},
                 {QStringLiteral("streams"),
                  QVariantList{
                      QVariantMap{
                          {QStringLiteral("stream_id"), 777u},
                          {QStringLiteral("node_id"), 888u},
                          {QStringLiteral("source_type"), QStringLiteral("monitor")},
                          {QStringLiteral("cursor_mode"), QStringLiteral("embedded")},
                          {QStringLiteral("session"), sessionPath},
                      },
                  }},
             }},
        };
    }
};

class FakeInputCaptureService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.InputCapture")

public:
    bool forceEnableFailure = false;
    QString forceEnableFailureReason = QStringLiteral("backend-failure");

public slots:
    QVariantMap CreateSession(const QString &sessionPath, const QString &, const QVariantMap &)
    {
        Session s;
        s.path = sessionPath;
        sessions.insert(sessionPath, s);
        return ok({
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("created"), true},
        });
    }

    QVariantMap SetPointerBarriers(const QString &sessionPath, const QVariantList &, const QVariantMap &)
    {
        if (!sessions.contains(sessionPath)) {
            return deny(QStringLiteral("missing-session-path"));
        }
        return ok({
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("barriers_set"), true},
        });
    }

    QVariantMap EnableSession(const QString &sessionPath, const QVariantMap &options)
    {
        if (!sessions.contains(sessionPath)) {
            return deny(QStringLiteral("missing-session-path"));
        }
        if (forceEnableFailure) {
            return deny(forceEnableFailureReason,
                        {{QStringLiteral("requires_mediation"), false}});
        }
        const bool allowPreempt = options.value(QStringLiteral("allow_preempt")).toBool();
        QString conflict;
        for (auto it = sessions.constBegin(); it != sessions.constEnd(); ++it) {
            if (it.key() == sessionPath) {
                continue;
            }
            if (it->enabled) {
                conflict = it.key();
                break;
            }
        }
        if (!conflict.isEmpty() && !allowPreempt) {
            return deny(QStringLiteral("conflict-active-session"),
                        {{QStringLiteral("conflict_session"), conflict},
                         {QStringLiteral("requires_mediation"), true}});
        }
        if (!conflict.isEmpty() && allowPreempt) {
            Session prev = sessions.value(conflict);
            prev.enabled = false;
            sessions.insert(conflict, prev);
        }
        Session s = sessions.value(sessionPath);
        s.enabled = true;
        sessions.insert(sessionPath, s);
        return ok({
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("enabled"), true},
        });
    }

    QVariantMap DisableSession(const QString &sessionPath, const QVariantMap &)
    {
        if (!sessions.contains(sessionPath)) {
            return deny(QStringLiteral("missing-session-path"));
        }
        Session s = sessions.value(sessionPath);
        s.enabled = false;
        sessions.insert(sessionPath, s);
        return ok({
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("disabled"), true},
        });
    }

    QVariantMap ReleaseSession(const QString &sessionPath, const QString &)
    {
        sessions.remove(sessionPath);
        return ok({
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("released"), true},
        });
    }

private:
    struct Session {
        QString path;
        bool enabled = false;
    };
    QHash<QString, Session> sessions;

    static QVariantMap ok(const QVariantMap &results)
    {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("response"), 0u},
            {QStringLiteral("results"), results},
        };
    }

    static QVariantMap deny(const QString &reason, const QVariantMap &results = {})
    {
        QVariantMap out = results;
        out.insert(QStringLiteral("reason"), reason);
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
            {QStringLiteral("reason"), reason},
            {QStringLiteral("response"), 2u},
            {QStringLiteral("results"), out},
        };
    }
};


class ImplPortalServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void pingAndInfo_contract();
    void openUri_dryRun_contract();
    void openUri_invalidInput_contract();
    void screenshot_screencast_invalidInput_contract();
    void splitInterfaces_areCallable();
    void fileChooser_successAndCancel_normalizedContract();
    void fileChooser_invalidMode_contract();
    void settings_read_contract();
    void notification_invalidInput_contract();
    void inhibit_invalidInput_contract();
    void documents_flow_contract();
    void screencast_start_prefersScreencastServiceStreams();
    void screencast_start_usesCaptureProviderStreams();
    void screencast_revoke_contract();
    void screencast_real_session_lifecycle_with_backend();
    void inputcapture_preempt_requiresTrustedMediation_contract();
    void inputcapture_preempt_trustedUiConsent_retrySucceeds_contract();
    void inputcapture_preempt_trustedUiConsent_denyAlways_contract();
    void inputcapture_enable_hostFailure_propagatesError_contract();
    void inputcapture_multisession_stability_contract();
    void inputcapture_postRelease_disableDenied_contract();

private:
    QVariant unmarshalAny(const QVariant &v) const
    {
        if (v.canConvert<QDBusArgument>()) {
            const QDBusArgument arg = v.value<QDBusArgument>();
            if (arg.currentType() == QDBusArgument::MapType) {
                QVariantMap map;
                arg >> map;
                for (auto it = map.begin(); it != map.end(); ++it) {
                    *it = unmarshalAny(*it);
                }
                return map;
            } else if (arg.currentType() == QDBusArgument::ArrayType) {
                QVariantList list;
                arg >> list;
                for (int i = 0; i < list.size(); ++i) {
                    list[i] = unmarshalAny(list[i]);
                }
                return list;
            }
        } else if (v.userType() == QMetaType::QVariantMap) {
            QVariantMap map = v.toMap();
            for (auto it = map.begin(); it != map.end(); ++it) {
                *it = unmarshalAny(*it);
            }
            return map;
        } else if (v.userType() == QMetaType::QVariantList) {
            QVariantList list = v.toList();
            for (int i = 0; i < list.size(); ++i) {
                list[i] = unmarshalAny(list[i]);
            }
            return list;
        }
        return v;
    }

    QVariantMap unmarshal(const QVariant &v) const
    {
        const QVariant unmarshalled = unmarshalAny(v);
        if (unmarshalled.userType() == QMetaType::QVariantMap) {
            return unmarshalled.toMap();
        }
        return v.toMap();
    }

    QDBusConnection m_bus = QDBusConnection::sessionBus();
    bool registerFakeUi(FakePortalUiService &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kUiService)).value()) {
            m_bus.unregisterObject(QString::fromLatin1(kUiPath));
            m_bus.unregisterService(QString::fromLatin1(kUiService));
        }
        if (!m_bus.registerService(QString::fromLatin1(kUiService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kUiPath),
                                  &fake,
                                  QDBusConnection::ExportAllSlots)) {
            m_bus.unregisterService(QString::fromLatin1(kUiService));
            return false;
        }
        return true;
    }
    void unregisterFakeUi()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
    }
    bool registerFakeCapture(FakeCaptureService &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kCaptureService)).value()) {
            m_bus.unregisterObject(QString::fromLatin1(kCapturePath));
            m_bus.unregisterService(QString::fromLatin1(kCaptureService));
        }
        if (!m_bus.registerService(QString::fromLatin1(kCaptureService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kCapturePath),
                                  &fake,
                                  QDBusConnection::ExportAllSlots)) {
            m_bus.unregisterService(QString::fromLatin1(kCaptureService));
            return false;
        }
        return true;
    }
    void unregisterFakeCapture()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kCapturePath));
        m_bus.unregisterService(QString::fromLatin1(kCaptureService));
    }
    bool registerFakeScreencast(FakeScreencastService &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kScreencastService)).value()) {
            m_bus.unregisterObject(QString::fromLatin1(kScreencastPath));
            m_bus.unregisterService(QString::fromLatin1(kScreencastService));
        }
        if (!m_bus.registerService(QString::fromLatin1(kScreencastService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kScreencastPath),
                                  &fake,
                                  QDBusConnection::ExportAllSlots)) {
            m_bus.unregisterService(QString::fromLatin1(kScreencastService));
            return false;
        }
        return true;
    }
    void unregisterFakeScreencast()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kScreencastPath));
        m_bus.unregisterService(QString::fromLatin1(kScreencastService));
    }
    bool registerFakeInputCapture(FakeInputCaptureService &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kInputCaptureService)).value()) {
            m_bus.unregisterObject(QString::fromLatin1(kInputCapturePath));
            m_bus.unregisterService(QString::fromLatin1(kInputCaptureService));
        }
        if (!m_bus.registerService(QString::fromLatin1(kInputCaptureService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kInputCapturePath),
                                  &fake,
                                  QDBusConnection::ExportAllSlots)) {
            m_bus.unregisterService(QString::fromLatin1(kInputCaptureService));
            return false;
        }
        return true;
    }
    void unregisterFakeInputCapture()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kInputCapturePath));
        m_bus.unregisterService(QString::fromLatin1(kInputCaptureService));
    }
};

void ImplPortalServiceDbusTest::pingAndInfo_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
    QVERIFY(pingReply.isValid());
    QVERIFY(pingReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(pingReply.value().value(QStringLiteral("service")).toString(),
             QString::fromLatin1(kService));

    QDBusReply<QVariantMap> infoReply = iface.call(QStringLiteral("GetBackendInfo"));
    QVERIFY(infoReply.isValid());
    const QVariantMap info = infoReply.value();
    QVERIFY(info.value(QStringLiteral("ok")).toBool());
    QCOMPARE(info.value(QStringLiteral("apiVersion")).toString(), QStringLiteral("0.1"));

    QDBusReply<QVariantMap> screencastStateReply = iface.call(QStringLiteral("GetScreencastState"));
    QVERIFY(screencastStateReply.isValid());
    const QVariantMap state = screencastStateReply.value();
    QVERIFY(state.value(QStringLiteral("ok")).toBool());
    const QVariantMap stateResults = unmarshal(state.value(QStringLiteral("results")));
    QVERIFY(stateResults.contains(QStringLiteral("active")));
    QVERIFY(stateResults.contains(QStringLiteral("active_count")));
    QVERIFY(stateResults.contains(QStringLiteral("active_sessions")));
    QVERIFY(stateResults.contains(QStringLiteral("active_apps")));
    QVERIFY(stateResults.contains(QStringLiteral("active_session_items")));

    QDBusReply<QVariantMap> closeInvalidReply =
        iface.call(QStringLiteral("CloseScreencastSession"), QStringLiteral(""));
    QVERIFY(closeInvalidReply.isValid());
    const QVariantMap closeInvalidOut = closeInvalidReply.value();
    QVERIFY(!closeInvalidOut.value(QStringLiteral("ok")).toBool());
    QCOMPARE(closeInvalidOut.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
}

void ImplPortalServiceDbusTest::openUri_dryRun_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    QVariantMap options;
    options.insert(QStringLiteral("dryRun"), true);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("OpenURI"),
                                               QStringLiteral("/request/1"),
                                               QStringLiteral("org.example.App"),
                                               QStringLiteral("x11:1"),
                                               QStringLiteral("https://example.com"),
                                               options);
    QVERIFY(reply.isValid());
    const QVariantMap out = reply.value();
    QVERIFY(out.value(QStringLiteral("ok")).toBool());
    QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 0u);
    const QVariantMap results = unmarshal(out.value(QStringLiteral("results")));
    QCOMPARE(results.value(QStringLiteral("uri")).toString(), QStringLiteral("https://example.com"));
    QCOMPARE(results.value(QStringLiteral("handled")).toBool(), true);
    QVERIFY(results.contains(QStringLiteral("request_handle")));
    QVERIFY(out.contains(QStringLiteral("request_handle")));
    QCOMPARE(out.value(QStringLiteral("uri")).toString(), QStringLiteral("https://example.com"));
    QCOMPARE(out.value(QStringLiteral("dryRun")).toBool(), true);
}

void ImplPortalServiceDbusTest::openUri_invalidInput_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIfaceOpenUri),
                         bus);
    QVERIFY(iface.isValid());

    {
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("OpenURI"),
                      QStringLiteral("/request/invalid-parent"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("gtk:abc"),
                      QStringLiteral("https://example.com"),
                      QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
        const QVariantMap results = unmarshal(out.value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("handled")).toBool(), false);
    }

    {
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("OpenURI"),
                      QStringLiteral("/request/invalid-uri"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      QStringLiteral(""),
                      QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
        const QVariantMap results = unmarshal(out.value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("handled")).toBool(), false);
    }
}

void ImplPortalServiceDbusTest::screenshot_screencast_invalidInput_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface screenshotIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenshot),
                                   bus);
    QVERIFY(screenshotIface.isValid());
    {
        QDBusReply<QVariantMap> reply =
            screenshotIface.call(QStringLiteral("Screenshot"),
                                 QStringLiteral(""),
                                 QStringLiteral("org.example.App"),
                                 QStringLiteral("x11:1"),
                                 QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
    }
    {
        QDBusReply<QVariantMap> reply =
            screenshotIface.call(QStringLiteral("Screenshot"),
                                 QStringLiteral("/request/screenshot-invalid-parent"),
                                 QStringLiteral("org.example.App"),
                                 QStringLiteral("gtk:bad"),
                                 QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
    }

    QDBusInterface screencastIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenCast),
                                   bus);
    QVERIFY(screencastIface.isValid());
    {
        QDBusReply<QVariantMap> reply =
            screencastIface.call(QStringLiteral("CreateSession"),
                                 QStringLiteral(""),
                                 QStringLiteral("org.example.App"),
                                 QStringLiteral("x11:1"),
                                 QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
    }
    {
        QDBusReply<QVariantMap> reply =
            screencastIface.call(QStringLiteral("SelectSources"),
                                 QStringLiteral("/request/select-invalid-session"),
                                 QStringLiteral("org.example.App"),
                                 QStringLiteral("x11:1"),
                                 QStringLiteral(""),
                                 QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
        QCOMPARE(unmarshal(out.value(QStringLiteral("results"))).value(QStringLiteral("sources_selected")).toBool(),
                 false);
    }
    {
        QDBusReply<QVariantMap> reply =
            screencastIface.call(QStringLiteral("Stop"),
                                 QStringLiteral("/request/stop-invalid-parent"),
                                 QStringLiteral("org.example.App"),
                                 QStringLiteral("gtk:bad"),
                                 QStringLiteral("/org/desktop/portal/session/org_example_App/s1"),
                                 QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
        QCOMPARE(unmarshal(out.value(QStringLiteral("results"))).value(QStringLiteral("stopped")).toBool(), false);
    }
}

void ImplPortalServiceDbusTest::splitInterfaces_areCallable()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    // org.freedesktop.impl.portal.OpenURI
    QDBusInterface openUriIface(QString::fromLatin1(kService),
                                QString::fromLatin1(kPath),
                                QString::fromLatin1(kIfaceOpenUri),
                                bus);
    QVERIFY(openUriIface.isValid());
    QVariantMap openUriOptions;
    openUriOptions.insert(QStringLiteral("dryRun"), true);
    QDBusReply<QVariantMap> openReply =
        openUriIface.call(QStringLiteral("OpenURI"),
                          QStringLiteral("/request/2"),
                          QStringLiteral("org.example.App"),
                          QStringLiteral("x11:1"),
                          QStringLiteral("https://example.com"),
                          openUriOptions);
    QVERIFY(openReply.isValid());
    QVERIFY(openReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(openReply.value().value(QStringLiteral("response")).toUInt(), 0u);

    // org.freedesktop.impl.portal.FileChooser (no fake UI here -> structured error expected).
    QDBusInterface chooserIface(QString::fromLatin1(kService),
                                QString::fromLatin1(kPath),
                                QString::fromLatin1(kIfaceFileChooser),
                                bus);
    QVERIFY(chooserIface.isValid());
    QVariantMap chooserOptions{{QStringLiteral("mode"), QStringLiteral("open")}};
    QDBusReply<QVariantMap> chooserReply =
        chooserIface.call(QStringLiteral("OpenFile"),
                          QStringLiteral("/request/3"),
                          QStringLiteral("org.example.App"),
                          QStringLiteral("x11:1"),
                          chooserOptions);
    QVERIFY(chooserReply.isValid());
    QVERIFY(!chooserReply.value().value(QStringLiteral("ok")).toBool());
    QVERIFY(!chooserReply.value().value(QStringLiteral("error")).toString().isEmpty());
    QVERIFY(chooserReply.value().contains(QStringLiteral("response")));
    QVERIFY(chooserReply.value().contains(QStringLiteral("results")));
    const QVariantMap chooserResults = unmarshal(chooserReply.value().value(QStringLiteral("results")));
    QVERIFY(chooserResults.contains(QStringLiteral("choices")));
    QVERIFY(chooserResults.contains(QStringLiteral("uris")));
    QVERIFY(chooserResults.contains(QStringLiteral("current_filter")));
    QVERIFY(chooserResults.contains(QStringLiteral("request_handle")));
    QVERIFY(chooserReply.value().contains(QStringLiteral("request_handle")));

    // org.freedesktop.impl.portal.Screenshot
    QDBusInterface screenshotIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenshot),
                                   bus);
    QVERIFY(screenshotIface.isValid());
    QDBusReply<QVariantMap> screenshotReply =
        screenshotIface.call(QStringLiteral("Screenshot"),
                             QStringLiteral("/request/4"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             QVariantMap{});
    QVERIFY(screenshotReply.isValid());
    QVERIFY(!screenshotReply.value().value(QStringLiteral("ok")).toBool());
    QVERIFY(screenshotReply.value().contains(QStringLiteral("response")));
    QVERIFY(unmarshal(screenshotReply.value().value(QStringLiteral("results")))
                .contains(QStringLiteral("uri")));
    QVERIFY(unmarshal(screenshotReply.value().value(QStringLiteral("results")))
                .contains(QStringLiteral("request_handle")));

    // org.freedesktop.impl.portal.ScreenCast (CreateSession path available).
    QDBusInterface screencastIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenCast),
                                   bus);
    QVERIFY(screencastIface.isValid());
    QDBusReply<QVariantMap> screencastReply =
        screencastIface.call(QStringLiteral("CreateSession"),
                             QStringLiteral("/request/5"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(screencastReply.isValid());
    QVERIFY(screencastReply.value().contains(QStringLiteral("ok")));
    QVERIFY(screencastReply.value().contains(QStringLiteral("response")));
    const QVariantMap createResults = unmarshal(screencastReply.value().value(QStringLiteral("results")));
    QVERIFY(createResults.contains(QStringLiteral("session_handle")));
    QVERIFY(createResults.contains(QStringLiteral("streams")));
    QVERIFY(createResults.contains(QStringLiteral("request_handle")));
    QVERIFY(screencastReply.value().contains(QStringLiteral("session_handle")));
    QVERIFY(screencastReply.value().contains(QStringLiteral("request_handle")));

    const QString sessionHandle = QStringLiteral("/org/desktop/portal/session/org_example_App/s1");
    QDBusReply<QVariantMap> selectReply =
        screencastIface.call(QStringLiteral("SelectSources"),
                             QStringLiteral("/request/6"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{});
    QVERIFY(selectReply.isValid());
    const QVariantMap selectResults = unmarshal(selectReply.value().value(QStringLiteral("results")));
    QCOMPARE(selectResults.value(QStringLiteral("session_handle")).toString(), sessionHandle);
    QVERIFY(selectResults.contains(QStringLiteral("sources_selected")));
    QCOMPARE(selectResults.value(QStringLiteral("sources_selected")).toBool(), false);
    QCOMPARE(selectReply.value().value(QStringLiteral("session_handle")).toString(), sessionHandle);

    QDBusReply<QVariantMap> startReply =
        screencastIface.call(QStringLiteral("Start"),
                             QStringLiteral("/request/7"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{});
    QVERIFY(startReply.isValid());
    const QVariantMap startResults = unmarshal(startReply.value().value(QStringLiteral("results")));
    QCOMPARE(startResults.value(QStringLiteral("session_handle")).toString(), sessionHandle);
    QVERIFY(startResults.contains(QStringLiteral("streams")));

    QDBusReply<QVariantMap> stopReply =
        screencastIface.call(QStringLiteral("Stop"),
                             QStringLiteral("/request/8"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{});
    QVERIFY(stopReply.isValid());
    const QVariantMap stopResults = unmarshal(stopReply.value().value(QStringLiteral("results")));
    QCOMPARE(stopResults.value(QStringLiteral("session_handle")).toString(), sessionHandle);
    QVERIFY(stopResults.contains(QStringLiteral("stopped")));
    QCOMPARE(stopResults.value(QStringLiteral("stopped")).toBool(), false);
    QCOMPARE(stopReply.value().value(QStringLiteral("session_handle")).toString(), sessionHandle);

    // org.freedesktop.impl.portal.GlobalShortcuts
    QDBusInterface shortcutsIface(QString::fromLatin1(kService),
                                  QString::fromLatin1(kPath),
                                  QString::fromLatin1(kIfaceGlobalShortcuts),
                                  bus);
    QVERIFY(shortcutsIface.isValid());

    const QString shortcutSession = QStringLiteral("/org/desktop/portal/session/org_example_App/gs1");
    QDBusReply<QVariantMap> gsCreateReply =
        shortcutsIface.call(QStringLiteral("CreateSession"),
                            QStringLiteral("/request/gs1"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            QVariantMap{
                                {QStringLiteral("initiatedByUserGesture"), true},
                                {QStringLiteral("sessionHandle"), shortcutSession},
                            });
    QVERIFY(gsCreateReply.isValid());
    QVERIFY(gsCreateReply.value().contains(QStringLiteral("ok")));
    QVERIFY(gsCreateReply.value().contains(QStringLiteral("response")));
    QVERIFY(gsCreateReply.value().contains(QStringLiteral("session_handle")));
    QVERIFY(unmarshal(gsCreateReply.value().value(QStringLiteral("results")))
                .contains(QStringLiteral("session_handle")));

    QDBusReply<QVariantMap> gsBindReply =
        shortcutsIface.call(QStringLiteral("BindShortcuts"),
                            QStringLiteral("/request/gs2"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            shortcutSession,
                            QVariantMap{
                                {QStringLiteral("initiatedByUserGesture"), true},
                                {QStringLiteral("bindings"), QVariantList{
                                     QVariantMap{
                                         {QStringLiteral("id"), QStringLiteral("toggle")},
                                         {QStringLiteral("accelerator"), QStringLiteral("Ctrl+Shift+Space")},
                                     },
                                 }},
                            });
    QVERIFY(gsBindReply.isValid());
    QVERIFY(gsBindReply.value().contains(QStringLiteral("ok")));
    QVERIFY(gsBindReply.value().contains(QStringLiteral("response")));
    QVERIFY(gsBindReply.value().contains(QStringLiteral("session_handle")));
    QVERIFY(unmarshal(gsBindReply.value().value(QStringLiteral("results")))
                .contains(QStringLiteral("session_handle")));
    QVERIFY(unmarshal(gsBindReply.value().value(QStringLiteral("results")))
                .contains(QStringLiteral("bound")));

    QDBusReply<QVariantMap> gsListReply =
        shortcutsIface.call(QStringLiteral("ListBindings"),
                            QStringLiteral("/request/gs3"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            shortcutSession,
                            QVariantMap{});
    QVERIFY(gsListReply.isValid());
    QVERIFY(gsListReply.value().contains(QStringLiteral("ok")));
    QVERIFY(gsListReply.value().contains(QStringLiteral("response")));
    QVERIFY(gsListReply.value().contains(QStringLiteral("session_handle")));
    const QVariantMap gsListResults = unmarshal(gsListReply.value().value(QStringLiteral("results")));
    QVERIFY(gsListResults.contains(QStringLiteral("bindings")));
    QVERIFY(gsListResults.contains(QStringLiteral("session_handle")));

    // org.freedesktop.impl.portal.InputCapture
    QDBusInterface inputCaptureIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceInputCapture),
                                     bus);
    QVERIFY(inputCaptureIface.isValid());
    const QString inputCaptureSession = QStringLiteral("/org/desktop/portal/session/org_example_App/ic1");
    QDBusReply<QVariantMap> icCreateReply =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/ic1"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), inputCaptureSession}});
    QVERIFY(icCreateReply.isValid());
    QVERIFY(icCreateReply.value().contains(QStringLiteral("ok")));
    QVERIFY(icCreateReply.value().contains(QStringLiteral("response")));
    QVERIFY(icCreateReply.value().contains(QStringLiteral("results")));

    QDBusReply<QVariantMap> icSetReply =
        inputCaptureIface.call(QStringLiteral("SetPointerBarriers"),
                               QStringLiteral("/request/ic2"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               inputCaptureSession,
                               QVariantMap{{QStringLiteral("barriers"), QVariantList{}}});
    QVERIFY(icSetReply.isValid());
    QVERIFY(icSetReply.value().contains(QStringLiteral("ok")));
    QVERIFY(icSetReply.value().contains(QStringLiteral("response")));
    QVERIFY(icSetReply.value().contains(QStringLiteral("results")));

    QDBusReply<QVariantMap> icEnableReply =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/ic3"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               inputCaptureSession,
                               QVariantMap{});
    QVERIFY(icEnableReply.isValid());
    QVERIFY(icEnableReply.value().contains(QStringLiteral("ok")));
    QVERIFY(icEnableReply.value().contains(QStringLiteral("response")));
    QVERIFY(icEnableReply.value().contains(QStringLiteral("results")));

    QDBusReply<QVariantMap> icDisableReply =
        inputCaptureIface.call(QStringLiteral("Disable"),
                               QStringLiteral("/request/ic4"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               inputCaptureSession,
                               QVariantMap{});
    QVERIFY(icDisableReply.isValid());
    QVERIFY(icDisableReply.value().contains(QStringLiteral("ok")));
    QVERIFY(icDisableReply.value().contains(QStringLiteral("response")));
    QVERIFY(icDisableReply.value().contains(QStringLiteral("results")));

    QDBusReply<QVariantMap> icReleaseReply =
        inputCaptureIface.call(QStringLiteral("Release"),
                               QStringLiteral("/request/ic5"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               inputCaptureSession,
                               QVariantMap{});
    QVERIFY(icReleaseReply.isValid());
    QVERIFY(icReleaseReply.value().contains(QStringLiteral("ok")));
    QVERIFY(icReleaseReply.value().contains(QStringLiteral("response")));
    QVERIFY(icReleaseReply.value().contains(QStringLiteral("results")));

    // org.freedesktop.impl.portal.Notification
    QDBusInterface notificationIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceNotification),
                                     bus);
    QVERIFY(notificationIface.isValid());
    QDBusReply<QVariantMap> notificationReply =
        notificationIface.call(QStringLiteral("AddNotification"),
                               QStringLiteral("org.example.App.desktop"),
                               QStringLiteral("notif-1"),
                               QVariantMap{{QStringLiteral("title"), QStringLiteral("hello")}});
    QVERIFY(notificationReply.isValid());
    QVERIFY(notificationReply.value().contains(QStringLiteral("ok")));
    QVERIFY(notificationReply.value().contains(QStringLiteral("response")));
    QVERIFY(notificationReply.value().contains(QStringLiteral("results")));

    // org.freedesktop.impl.portal.Inhibit
    QDBusInterface inhibitIface(QString::fromLatin1(kService),
                                QString::fromLatin1(kPath),
                                QString::fromLatin1(kIfaceInhibit),
                                bus);
    QVERIFY(inhibitIface.isValid());
    QDBusReply<QVariantMap> inhibitReply =
        inhibitIface.call(QStringLiteral("Inhibit"),
                          QStringLiteral("/request/11"),
                          QStringLiteral("org.example.App.desktop"),
                          QStringLiteral("x11:1"),
                          uint(0),
                          QVariantMap{{QStringLiteral("reason"), QStringLiteral("split-interface-test")}});
    QVERIFY(inhibitReply.isValid());
    QVERIFY(inhibitReply.value().contains(QStringLiteral("ok")));
    QVERIFY(inhibitReply.value().contains(QStringLiteral("response")));
    QVERIFY(inhibitReply.value().contains(QStringLiteral("results")));

    // org.freedesktop.impl.portal.OpenWith
    QDBusInterface openWithIface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIfaceOpenWith),
                                 bus);
    QVERIFY(openWithIface.isValid());
    const QString samplePath = QStringLiteral("/etc/hosts");
    QDBusReply<QVariantMap> handlersReply =
        openWithIface.call(QStringLiteral("QueryHandlers"),
                           QStringLiteral("/request/12"),
                           QStringLiteral("org.example.App.desktop"),
                           QStringLiteral("x11:1"),
                           samplePath,
                           QVariantMap{{QStringLiteral("limit"), 8}});
    QVERIFY(handlersReply.isValid());
    QVERIFY(handlersReply.value().contains(QStringLiteral("ok")));
    QVERIFY(handlersReply.value().contains(QStringLiteral("response")));
    QVERIFY(handlersReply.value().contains(QStringLiteral("results")));
    const QVariantMap handlersResults = unmarshal(handlersReply.value().value(QStringLiteral("results")));
    QVERIFY(handlersResults.contains(QStringLiteral("handlers")));
    const QVariantList handlers = handlersResults.value(QStringLiteral("handlers")).toList();
    QString handlerId = handlersResults.value(QStringLiteral("default_handler")).toString();
    if (handlerId.isEmpty() && !handlers.isEmpty()) {
        handlerId = unmarshal(handlers.constFirst()).value(QStringLiteral("id")).toString();
    }
    if (handlerId.isEmpty()) {
        handlerId = QStringLiteral("org.example.Dummy.desktop");
    }
    QDBusReply<QVariantMap> openWithReply =
        openWithIface.call(QStringLiteral("OpenFileWith"),
                           QStringLiteral("/request/13"),
                           QStringLiteral("org.example.App.desktop"),
                           QStringLiteral("x11:1"),
                           samplePath,
                           handlerId,
                           QVariantMap{{QStringLiteral("dryRun"), true}});
    QVERIFY(openWithReply.isValid());
    const QVariantMap openWithResults = unmarshal(openWithReply.value().value(QStringLiteral("results")));
    QVERIFY(openWithResults.contains(QStringLiteral("handler")));
    QVERIFY(openWithResults.contains(QStringLiteral("dryRun")));
}

void ImplPortalServiceDbusTest::fileChooser_successAndCancel_normalizedContract()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }
    FakePortalUiService fake;
    if (!registerFakeUi(fake)) {
        QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface chooserIface(QString::fromLatin1(kService),
                                QString::fromLatin1(kPath),
                                QString::fromLatin1(kIfaceFileChooser),
                                m_bus);
    QVERIFY(chooserIface.isValid());

    // success payload with local path should normalize to uris + response=0
    fake.response = {
        {QStringLiteral("ok"), true},
        {QStringLiteral("path"), QStringLiteral("/home/garis/Pictures/a.png")},
        {QStringLiteral("mode"), QStringLiteral("open")},
    };
    QDBusReply<QVariantMap> successReply =
        chooserIface.call(QStringLiteral("OpenFile"),
                          QStringLiteral("/request/9"),
                          QStringLiteral("org.example.App"),
                          QStringLiteral("x11:1"),
                          QVariantMap{{QStringLiteral("mode"), QStringLiteral("open")}});
    QVERIFY(successReply.isValid());
    const QVariantMap successOut = successReply.value();
    QCOMPARE(successOut.value(QStringLiteral("response")).toUInt(), 0u);
    const QVariantMap successResults = unmarshal(successOut.value(QStringLiteral("results")));
    qDebug() << "Test successResults keys:" << successResults.keys();
    qDebug() << "Test successResults uris type:" << successResults.value(QStringLiteral("uris")).typeName();
    qDebug() << "Test successResults uris value:" << successResults.value(QStringLiteral("uris"));
    const QVariantList successUris = successResults.value(QStringLiteral("uris")).toList();
    QCOMPARE(successUris.size(), 1);
    QCOMPARE(successUris.first().toString(), QStringLiteral("file:///home/garis/Pictures/a.png"));

    // canceled payload should normalize to response=2 and keep deterministic keys.
    fake.response = {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("canceled")},
        {QStringLiteral("canceled"), true},
    };
    QDBusReply<QVariantMap> cancelReply =
        chooserIface.call(QStringLiteral("OpenFile"),
                          QStringLiteral("/request/10"),
                          QStringLiteral("org.example.App"),
                          QStringLiteral("x11:1"),
                          QVariantMap{{QStringLiteral("mode"), QStringLiteral("open")}});
    QVERIFY(cancelReply.isValid());
    const QVariantMap cancelOut = cancelReply.value();
    QCOMPARE(cancelOut.value(QStringLiteral("response")).toUInt(), 2u);
    const QVariantMap cancelResults = unmarshal(cancelOut.value(QStringLiteral("results")));
    QVERIFY(cancelResults.contains(QStringLiteral("uris")));
    QVERIFY(cancelResults.contains(QStringLiteral("choices")));
    QVERIFY(cancelResults.contains(QStringLiteral("request_handle")));

    unregisterFakeUi();
}

void ImplPortalServiceDbusTest::screencast_start_usesCaptureProviderStreams()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakeCaptureService fakeCapture;
    if (!registerFakeCapture(fakeCapture)) {
        QSKIP("cannot register fake org.slm.Desktop.Capture service/object");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface screencastIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenCast),
                                   m_bus);
    QVERIFY(screencastIface.isValid());

    const QString sessionHandle = QStringLiteral("/org/desktop/portal/session/org_example_App/provider");
    QDBusReply<QVariantMap> startReply =
        screencastIface.call(QStringLiteral("Start"),
                             QStringLiteral("/request/provider-start"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{});
    QVERIFY(startReply.isValid());
    const QVariantMap startOut = startReply.value();
    const QVariantMap startResults = unmarshal(startOut.value(QStringLiteral("results")));
    const QVariantList streams = startResults.value(QStringLiteral("streams")).toList();
    QVERIFY(!streams.isEmpty());
    const QVariantMap firstStream = unmarshal(streams.constFirst());
    QCOMPARE(firstStream.value(QStringLiteral("stream_id")).toUInt(), 314u);
    QCOMPARE(firstStream.value(QStringLiteral("node_id")).toUInt(), 2718u);
    QCOMPARE(firstStream.value(QStringLiteral("source_type")).toString(), QStringLiteral("window"));
    QCOMPARE(firstStream.value(QStringLiteral("cursor_mode")).toString(), QStringLiteral("metadata"));
    QCOMPARE(startResults.value(QStringLiteral("stream_provider")).toString(),
             QStringLiteral("capture-service"));

    unregisterFakeCapture();
}

void ImplPortalServiceDbusTest::screencast_start_prefersScreencastServiceStreams()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakeScreencastService fakeScreencast;
    if (!registerFakeScreencast(fakeScreencast)) {
        QSKIP("cannot register fake org.slm.Desktop.Screencast service/object");
    }

    FakeCaptureService fakeCapture;
    if (!registerFakeCapture(fakeCapture)) {
        unregisterFakeScreencast();
        QSKIP("cannot register fake org.slm.Desktop.Capture service/object");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface screencastIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenCast),
                                   m_bus);
    QVERIFY(screencastIface.isValid());

    const QString sessionHandle = QStringLiteral("/org/desktop/portal/session/org_example_App/provider-pref");
    QDBusReply<QVariantMap> startReply =
        screencastIface.call(QStringLiteral("Start"),
                             QStringLiteral("/request/provider-pref-start"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{});
    QVERIFY(startReply.isValid());
    const QVariantMap startOut = startReply.value();
    const QVariantMap startResults = unmarshal(startOut.value(QStringLiteral("results")));
    const QVariantList streams = startResults.value(QStringLiteral("streams")).toList();
    QVERIFY(!streams.isEmpty());
    const QVariantMap firstStream = unmarshal(streams.constFirst());
    QCOMPARE(firstStream.value(QStringLiteral("stream_id")).toUInt(), 777u);
    QCOMPARE(firstStream.value(QStringLiteral("node_id")).toUInt(), 888u);
    QCOMPARE(startResults.value(QStringLiteral("stream_provider")).toString(),
             QStringLiteral("screencast-service"));
    QVERIFY(fakeScreencast.getSessionStreamsCalls > 0);
    QCOMPARE(fakeCapture.getStreamsCalls, 0);

    unregisterFakeCapture();
    unregisterFakeScreencast();
}

void ImplPortalServiceDbusTest::screencast_revoke_contract()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakeCaptureService fakeCapture;
    if (!registerFakeCapture(fakeCapture)) {
        QSKIP("cannot register fake org.slm.Desktop.Capture service/object");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface rootIface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             m_bus);
    QVERIFY(rootIface.isValid());

    QDBusInterface screencastIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenCast),
                                   m_bus);
    QVERIFY(screencastIface.isValid());

    const QString sessionHandle = QStringLiteral("/org/desktop/portal/session/org_example_App/revoke1");

    QDBusReply<QVariantMap> createReply =
        screencastIface.call(QStringLiteral("CreateSession"),
                             QStringLiteral("/request/revoke-create"),
                             QStringLiteral("org.example.App"),
                             QStringLiteral("x11:1"),
                             QVariantMap{{QStringLiteral("sessionPath"), sessionHandle},
                                         {QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(createReply.isValid());
    QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> revokeReply =
        rootIface.call(QStringLiteral("RevokeScreencastSession"),
                       sessionHandle,
                       QStringLiteral("policy-revoke"));
    QVERIFY(revokeReply.isValid());
    const QVariantMap revokeOut = revokeReply.value();
    QVERIFY(revokeOut.value(QStringLiteral("ok")).toBool());
    QCOMPARE(revokeOut.value(QStringLiteral("response")).toUInt(), 0u);
    const QVariantMap revokeResults = unmarshal(revokeOut.value(QStringLiteral("results")));
    QCOMPARE(revokeResults.value(QStringLiteral("revoked")).toBool(), true);
    QCOMPARE(revokeResults.value(QStringLiteral("session_handle")).toString(), sessionHandle);
    QCOMPARE(revokeResults.value(QStringLiteral("reason")).toString(), QStringLiteral("policy-revoke"));
    QCOMPARE(revokeResults.value(QStringLiteral("state")).toString(), QStringLiteral("Revoked"));
    QCOMPARE(fakeCapture.revokeCalls, 1);
    QCOMPARE(fakeCapture.lastRevokedSession, sessionHandle);
    QCOMPARE(fakeCapture.lastRevokeReason, QStringLiteral("policy-revoke"));

    QDBusReply<QVariantMap> invalidReply =
        rootIface.call(QStringLiteral("RevokeScreencastSession"),
                       QStringLiteral(""),
                       QStringLiteral("policy-revoke"));
    QVERIFY(invalidReply.isValid());
    QVERIFY(!invalidReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(invalidReply.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("invalid-argument"));

    unregisterFakeCapture();
}

void ImplPortalServiceDbusTest::screencast_real_session_lifecycle_with_backend()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    Slm::Permissions::PermissionStore store;
    QVERIFY(store.open(tmp.filePath(QStringLiteral("permissions.db"))));

    Slm::Permissions::PolicyEngine policyEngine;
    policyEngine.setStore(&store);
    Slm::Permissions::PermissionBroker permissionBroker;
    permissionBroker.setStore(&store);
    permissionBroker.setPolicyEngine(&policyEngine);
    Slm::Permissions::TrustResolver trustResolver;

    const QString appId = QStringLiteral("org.example.App");
    QVERIFY(store.savePermission(appId,
                                 Slm::Permissions::Capability::ScreencastCreateSession,
                                 Slm::Permissions::DecisionType::AllowAlways,
                                 QStringLiteral("persistent")));
    QVERIFY(store.savePermission(appId,
                                 Slm::Permissions::Capability::ScreencastSelectSources,
                                 Slm::Permissions::DecisionType::AllowAlways,
                                 QStringLiteral("persistent")));
    QVERIFY(store.savePermission(appId,
                                 Slm::Permissions::Capability::ScreencastStart,
                                 Slm::Permissions::DecisionType::AllowAlways,
                                 QStringLiteral("persistent")));
    QVERIFY(store.savePermission(appId,
                                 Slm::Permissions::Capability::ScreencastStop,
                                 Slm::Permissions::DecisionType::AllowAlways,
                                 QStringLiteral("persistent")));

    Slm::PortalAdapter::PortalBackendService backend;
    backend.setBus(m_bus);
    backend.configurePermissions(&trustResolver, &permissionBroker, nullptr, &store);

    PortalManager manager;
    ImplPortalService service(&manager, &backend, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface screencastIface(QString::fromLatin1(kService),
                                   QString::fromLatin1(kPath),
                                   QString::fromLatin1(kIfaceScreenCast),
                                   m_bus);
    QVERIFY(screencastIface.isValid());

    QDBusInterface rootIface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             m_bus);
    QVERIFY(rootIface.isValid());

    const QString sessionHandle =
        QStringLiteral("/org/desktop/portal/session/org_example_App/real-flow");

    QDBusReply<QVariantMap> createReply =
        screencastIface.call(QStringLiteral("CreateSession"),
                             QStringLiteral("/request/real-create"),
                             appId,
                             QStringLiteral("x11:1"),
                             QVariantMap{
                                 {QStringLiteral("sessionPath"), sessionHandle},
                                 {QStringLiteral("initiatedByUserGesture"), true},
                             });
    QVERIFY(createReply.isValid());
    QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> selectReply =
        screencastIface.call(QStringLiteral("SelectSources"),
                             QStringLiteral("/request/real-select"),
                             appId,
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{
                                 {QStringLiteral("sources"), QVariantList{QStringLiteral("monitor")}},
                                 {QStringLiteral("initiatedByUserGesture"), true},
                             });
    QVERIFY(selectReply.isValid());
    QVERIFY(selectReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> startReply =
        screencastIface.call(QStringLiteral("Start"),
                             QStringLiteral("/request/real-start"),
                             appId,
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{
                                 {QStringLiteral("initiatedByUserGesture"), true},
                             });
    QVERIFY(startReply.isValid());
    QVERIFY(startReply.value().value(QStringLiteral("ok")).toBool());
    const QVariantMap startResults = unmarshal(startReply.value().value(QStringLiteral("results")));
    QVERIFY(startResults.value(QStringLiteral("streams")).toList().size() > 0);

    {
        QDBusReply<QVariantMap> stateReply = rootIface.call(QStringLiteral("GetScreencastState"));
        QVERIFY(stateReply.isValid());
        QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = unmarshal(stateReply.value().value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("active_count")).toInt(), 1);
        QCOMPARE(results.value(QStringLiteral("active")).toBool(), true);
    }

    QDBusReply<QVariantMap> stopReply =
        screencastIface.call(QStringLiteral("Stop"),
                             QStringLiteral("/request/real-stop"),
                             appId,
                             QStringLiteral("x11:1"),
                             sessionHandle,
                             QVariantMap{
                                 {QStringLiteral("closeSession"), true},
                                 {QStringLiteral("initiatedByUserGesture"), true},
                             });
    QVERIFY(stopReply.isValid());
    QVERIFY(stopReply.value().value(QStringLiteral("ok")).toBool());
    const QVariantMap stopResults = unmarshal(stopReply.value().value(QStringLiteral("results")));
    QCOMPARE(stopResults.value(QStringLiteral("stopped")).toBool(), true);
    QCOMPARE(stopResults.value(QStringLiteral("session_closed")).toBool(), true);

    {
        QDBusReply<QVariantMap> stateReply = rootIface.call(QStringLiteral("GetScreencastState"));
        QVERIFY(stateReply.isValid());
        QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = unmarshal(stateReply.value().value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("active_count")).toInt(), 0);
        QCOMPARE(results.value(QStringLiteral("active")).toBool(), false);
    }
}

void ImplPortalServiceDbusTest::fileChooser_invalidMode_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface chooserIface(QString::fromLatin1(kService),
                                QString::fromLatin1(kPath),
                                QString::fromLatin1(kIfaceFileChooser),
                                bus);
    QVERIFY(chooserIface.isValid());

    QVariantMap options{{QStringLiteral("mode"), QStringLiteral("folder")}};
    QDBusReply<QVariantMap> reply =
        chooserIface.call(QStringLiteral("OpenFile"),
                          QStringLiteral("/request/invalid-mode"),
                          QStringLiteral("org.example.App"),
                          QStringLiteral("x11:1"),
                          options);
    QVERIFY(reply.isValid());
    const QVariantMap out = reply.value();
    QVERIFY(!out.value(QStringLiteral("ok")).toBool());
    QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
    QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
    const QVariantMap results = unmarshal(out.value(QStringLiteral("results")));
    QVERIFY(results.contains(QStringLiteral("choices")));
    QVERIFY(results.contains(QStringLiteral("uris")));
    QVERIFY(results.contains(QStringLiteral("current_filter")));
}

void ImplPortalServiceDbusTest::settings_read_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface settingsIface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIfaceSettings),
                                 bus);
    QVERIFY(settingsIface.isValid());

    QDBusReply<QVariantMap> allReply =
        settingsIface.call(QStringLiteral("ReadAll"),
                           QStringList{QStringLiteral("org.freedesktop.appearance")});
    QVERIFY(allReply.isValid());
    const QVariantMap all = allReply.value();
    QVERIFY(all.contains(QStringLiteral("org.freedesktop.appearance")));
    const QVariantMap appearance = unmarshal(all.value(QStringLiteral("org.freedesktop.appearance")));
    QVERIFY(appearance.contains(QStringLiteral("color-scheme")));

    QDBusReply<QDBusVariant> oneReply =
        settingsIface.call(QStringLiteral("Read"),
                           QStringLiteral("org.freedesktop.appearance"),
                           QStringLiteral("color-scheme"));
    QVERIFY(oneReply.isValid());
    QVERIFY(oneReply.value().variant().isValid());
}

void ImplPortalServiceDbusTest::notification_invalidInput_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface notifIface(QString::fromLatin1(kService),
                              QString::fromLatin1(kPath),
                              QString::fromLatin1(kIfaceNotification),
                              bus);
    QVERIFY(notifIface.isValid());

    QDBusReply<QVariantMap> addReply =
        notifIface.call(QStringLiteral("AddNotification"),
                        QStringLiteral("org.example.App.desktop"),
                        QStringLiteral(""),
                        QVariantMap{{QStringLiteral("title"), QStringLiteral("t")}});
    QVERIFY(addReply.isValid());
    const QVariantMap out = addReply.value();
    QVERIFY(!out.value(QStringLiteral("ok")).toBool());
    QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
    QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
}

void ImplPortalServiceDbusTest::inhibit_invalidInput_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface inhibitIface(QString::fromLatin1(kService),
                                QString::fromLatin1(kPath),
                                QString::fromLatin1(kIfaceInhibit),
                                bus);
    QVERIFY(inhibitIface.isValid());

    QDBusReply<QVariantMap> reply =
        inhibitIface.call(QStringLiteral("Inhibit"),
                          QStringLiteral(""), // missing handle
                          QStringLiteral("org.example.App.desktop"),
                          QStringLiteral("x11:1"),
                          uint(0),
                          QVariantMap{{QStringLiteral("reason"), QStringLiteral("test")}});
    QVERIFY(reply.isValid());
    const QVariantMap out = reply.value();
    QVERIFY(!out.value(QStringLiteral("ok")).toBool());
    QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
    QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
}

void ImplPortalServiceDbusTest::documents_flow_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface docsIface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceDocuments),
                             bus);
    QVERIFY(docsIface.isValid());

    QDBusReply<QVariantMap> addReply =
        docsIface.call(QStringLiteral("Add"),
                       QStringLiteral("/request/doc-1"),
                       QStringLiteral("org.example.App.desktop"),
                       QStringLiteral("x11:1"),
                       QStringList{QStringLiteral("file:///etc/hosts")},
                       QVariantMap{});
    QVERIFY(addReply.isValid());
    QVERIFY(addReply.value().value(QStringLiteral("ok")).toBool());
    const QVariantList docs = unmarshal(addReply.value().value(QStringLiteral("results")))
                                  .value(QStringLiteral("documents")).toList();
    QVERIFY(!docs.isEmpty());
    const QString token = unmarshal(docs.constFirst()).value(QStringLiteral("token")).toString();
    QVERIFY(!token.isEmpty());

    QDBusReply<QVariantMap> resolveReply =
        docsIface.call(QStringLiteral("Resolve"),
                       QStringLiteral("/request/doc-2"),
                       QStringLiteral("org.example.App.desktop"),
                       QStringLiteral("x11:1"),
                       token,
                       QVariantMap{});
    QVERIFY(resolveReply.isValid());
    QVERIFY(resolveReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> removeReply =
        docsIface.call(QStringLiteral("Remove"),
                       QStringLiteral("/request/doc-3"),
                       QStringLiteral("org.example.App.desktop"),
                       QStringLiteral("x11:1"),
                       token,
                       QVariantMap{});
    QVERIFY(removeReply.isValid());
    QVERIFY(removeReply.value().value(QStringLiteral("ok")).toBool());

    QDBusInterface trashIface(QString::fromLatin1(kService),
                              QString::fromLatin1(kPath),
                              QString::fromLatin1(kIfaceTrash),
                              bus);
    QVERIFY(trashIface.isValid());
    QDBusReply<QVariantMap> trashReply =
        trashIface.call(QStringLiteral("TrashFile"),
                        QStringLiteral("/request/trash-1"),
                        QStringLiteral("org.example.App.desktop"),
                        QStringLiteral("x11:1"),
                        QStringLiteral(""),
                        QVariantMap{});
    QVERIFY(trashReply.isValid());
    QVERIFY(!trashReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(trashReply.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("invalid-argument"));
}

void ImplPortalServiceDbusTest::inputcapture_preempt_requiresTrustedMediation_contract()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakeInputCaptureService fakeInputCapture;
    if (!registerFakeInputCapture(fakeInputCapture)) {
        QSKIP("cannot register fake input capture service");
    }
    auto inputCaptureCleanup = qScopeGuard([&]() {
        unregisterFakeInputCapture();
    });

    PortalManager manager;
    Slm::PortalAdapter::PortalBackendService backend;
    ImplPortalService service(&manager, &backend);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register impl portal service name on session bus (likely already owned)");
    }

    QDBusInterface inputCaptureIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceInputCapture),
                                     bus);
    QVERIFY(inputCaptureIface.isValid());

    const QString session1 = QStringLiteral("/org/desktop/portal/session/org_example_App/icp1");
    const QString session2 = QStringLiteral("/org/desktop/portal/session/org_example_App/icp2");

    QDBusReply<QVariantMap> create1 =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/icp1"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), session1}});
    QVERIFY(create1.isValid());
    QVERIFY(create1.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> create2 =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/icp2"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), session2}});
    QVERIFY(create2.isValid());
    QVERIFY(create2.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable1 =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/icp3"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               session1,
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(enable1.isValid());
    QVERIFY(enable1.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable2 =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/icp4"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               session2,
                               QVariantMap{
                                   {QStringLiteral("initiatedByUserGesture"), true},
                                   {QStringLiteral("initiatedFromOfficialUI"), true},
                                   {QStringLiteral("allow_preempt"), true},
                               });
    QVERIFY(enable2.isValid());
    QVERIFY(!enable2.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(enable2.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));
    QCOMPARE(enable2.value().value(QStringLiteral("reason")).toString(),
             QStringLiteral("preempt-requires-trusted-mediation"));
    const QVariantMap results = unmarshal(enable2.value().value(QStringLiteral("results")));
    QCOMPARE(results.value(QStringLiteral("requires_mediation")).toBool(), true);
    QCOMPARE(results.value(QStringLiteral("mediation_action")).toString(),
             QStringLiteral("inputcapture-preempt-consent"));
    QCOMPARE(results.value(QStringLiteral("mediation_scope")).toString(),
             QStringLiteral("session-conflict"));
    const QVariantMap mediation = unmarshal(results.value(QStringLiteral("mediation")));
    QCOMPARE(mediation.value(QStringLiteral("required")).toBool(), true);
    QCOMPARE(mediation.value(QStringLiteral("action")).toString(),
             QStringLiteral("inputcapture-preempt-consent"));
    QCOMPARE(mediation.value(QStringLiteral("scope")).toString(),
             QStringLiteral("session-conflict"));
    QCOMPARE(mediation.value(QStringLiteral("reason")).toString(),
             QStringLiteral("preempt-requires-trusted-mediation"));
}

void ImplPortalServiceDbusTest::inputcapture_preempt_trustedUiConsent_retrySucceeds_contract()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakePortalUiService fakeUi;
    fakeUi.consentResponse = {
        {QStringLiteral("decision"), QStringLiteral("allow_always")},
        {QStringLiteral("persist"), true},
        {QStringLiteral("scope"), QStringLiteral("persistent")},
        {QStringLiteral("reason"), QStringLiteral("trusted-ui-approved")},
    };
    if (!registerFakeUi(fakeUi)) {
        QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
    }
    auto uiCleanup = qScopeGuard([&]() {
        unregisterFakeUi();
    });

    FakeInputCaptureService fakeInputCapture;
    if (!registerFakeInputCapture(fakeInputCapture)) {
        QSKIP("cannot register fake input capture service");
    }
    auto inputCaptureCleanup = qScopeGuard([&]() {
        unregisterFakeInputCapture();
    });

    PortalManager manager;
    Slm::PortalAdapter::PortalBackendService backend;
    ImplPortalService service(&manager, &backend);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register impl portal service name on session bus (likely already owned)");
    }

    QDBusInterface inputCaptureIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceInputCapture),
                                     bus);
    QVERIFY(inputCaptureIface.isValid());

    const QString session1 = QStringLiteral("/org/desktop/portal/session/org_example_App/icpc1");
    const QString session2 = QStringLiteral("/org/desktop/portal/session/org_example_App/icpc2");

    QDBusReply<QVariantMap> create1 =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/icpc1"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), session1}});
    QVERIFY(create1.isValid());
    QVERIFY(create1.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> create2 =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/icpc2"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), session2}});
    QVERIFY(create2.isValid());
    QVERIFY(create2.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable1 =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/icpc3"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               session1,
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(enable1.isValid());
    QVERIFY(enable1.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable2 =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/icpc4"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               session2,
                               QVariantMap{
                                   {QStringLiteral("initiatedByUserGesture"), true},
                                   {QStringLiteral("allow_preempt"), true},
                               });
    QVERIFY(enable2.isValid());
    QVERIFY(enable2.value().value(QStringLiteral("ok")).toBool());
    const QVariantMap results = unmarshal(enable2.value().value(QStringLiteral("results")));
    QCOMPARE(results.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(results.value(QStringLiteral("host_preempt_retry_via_consent")).toBool(), true);
    QCOMPARE(results.value(QStringLiteral("host_synced")).toBool(), true);
    const QVariantMap policy = unmarshal(results.value(QStringLiteral("host_preempt_mediation_policy")));
    QCOMPARE(policy.value(QStringLiteral("decision")).toString(), QStringLiteral("allow_always"));
    QCOMPARE(policy.value(QStringLiteral("persisted")).toBool(), true);
    QCOMPARE(policy.value(QStringLiteral("persist_scope")).toString(), QStringLiteral("persistent"));
}

void ImplPortalServiceDbusTest::inputcapture_preempt_trustedUiConsent_denyAlways_contract()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakePortalUiService fakeUi;
    fakeUi.consentResponse = {
        {QStringLiteral("decision"), QStringLiteral("deny_always")},
        {QStringLiteral("persist"), true},
        {QStringLiteral("scope"), QStringLiteral("persistent")},
        {QStringLiteral("reason"), QStringLiteral("trusted-ui-denied")},
    };
    if (!registerFakeUi(fakeUi)) {
        QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
    }
    auto uiCleanup = qScopeGuard([&]() {
        unregisterFakeUi();
    });

    FakeInputCaptureService fakeInputCapture;
    if (!registerFakeInputCapture(fakeInputCapture)) {
        QSKIP("cannot register fake input capture service");
    }
    auto inputCaptureCleanup = qScopeGuard([&]() {
        unregisterFakeInputCapture();
    });

    PortalManager manager;
    Slm::PortalAdapter::PortalBackendService backend;
    ImplPortalService service(&manager, &backend);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register impl portal service name on session bus (likely already owned)");
    }

    QDBusInterface inputCaptureIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceInputCapture),
                                     bus);
    QVERIFY(inputCaptureIface.isValid());

    const QString session1 = QStringLiteral("/org/desktop/portal/session/org_example_App/icpd1");
    const QString session2 = QStringLiteral("/org/desktop/portal/session/org_example_App/icpd2");

    QDBusReply<QVariantMap> create1 =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/icpd1"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), session1}});
    QVERIFY(create1.isValid());
    QVERIFY(create1.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> create2 =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/icpd2"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), session2}});
    QVERIFY(create2.isValid());
    QVERIFY(create2.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable1 =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/icpd3"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               session1,
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(enable1.isValid());
    QVERIFY(enable1.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable2 =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/icpd4"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               session2,
                               QVariantMap{
                                   {QStringLiteral("initiatedByUserGesture"), true},
                                   {QStringLiteral("allow_preempt"), true},
                               });
    QVERIFY(enable2.isValid());
    QVERIFY(!enable2.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(enable2.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));
    const QVariantMap results = unmarshal(enable2.value().value(QStringLiteral("results")));
    const QVariantMap policy = unmarshal(results.value(QStringLiteral("host_preempt_mediation_policy")));
    QCOMPARE(policy.value(QStringLiteral("decision")).toString(), QStringLiteral("deny_always"));
    QCOMPARE(policy.value(QStringLiteral("persisted")).toBool(), true);
    QCOMPARE(policy.value(QStringLiteral("persist_scope")).toString(), QStringLiteral("persistent"));
}

void ImplPortalServiceDbusTest::inputcapture_enable_hostFailure_propagatesError_contract()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakeInputCaptureService fakeInputCapture;
    fakeInputCapture.forceEnableFailure = true;
    fakeInputCapture.forceEnableFailureReason = QStringLiteral("host-enable-failed");
    if (!registerFakeInputCapture(fakeInputCapture)) {
        QSKIP("cannot register fake input capture service");
    }
    auto inputCaptureCleanup = qScopeGuard([&]() {
        unregisterFakeInputCapture();
    });

    PortalManager manager;
    Slm::PortalAdapter::PortalBackendService backend;
    ImplPortalService service(&manager, &backend);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register impl portal service name on session bus (likely already owned)");
    }

    QDBusInterface inputCaptureIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceInputCapture),
                                     bus);
    QVERIFY(inputCaptureIface.isValid());

    const QString sessionPath = QStringLiteral("/org/desktop/portal/session/org_example_App/ic_host_fail");
    QDBusReply<QVariantMap> create =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/ic_host_fail_create"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), sessionPath}});
    QVERIFY(create.isValid());
    QVERIFY(create.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable =
        inputCaptureIface.call(QStringLiteral("Enable"),
                               QStringLiteral("/request/ic_host_fail_enable"),
                               QStringLiteral("org.example.App"),
                               QStringLiteral("x11:1"),
                               sessionPath,
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(enable.isValid());
    QVERIFY(!enable.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(enable.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));
    QCOMPARE(enable.value().value(QStringLiteral("reason")).toString(),
             QStringLiteral("host-enable-failed"));
}

void ImplPortalServiceDbusTest::inputcapture_multisession_stability_contract()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakeInputCaptureService fakeInputCapture;
    if (!registerFakeInputCapture(fakeInputCapture)) {
        QSKIP("cannot register fake input capture service");
    }
    auto inputCaptureCleanup = qScopeGuard([&]() {
        unregisterFakeInputCapture();
    });

    PortalManager manager;
    Slm::PortalAdapter::PortalBackendService backend;
    ImplPortalService service(&manager, &backend);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register impl portal service name on session bus (likely already owned)");
    }

    QDBusInterface inputCaptureIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceInputCapture),
                                     bus);
    QVERIFY(inputCaptureIface.isValid());

    const QString appId = QStringLiteral("org.example.App");
    const QString parent = QStringLiteral("x11:1");
    const QString s1 = QStringLiteral("/org/desktop/portal/session/org_example_App/ics1");
    const QString s2 = QStringLiteral("/org/desktop/portal/session/org_example_App/ics2");
    const QString s3 = QStringLiteral("/org/desktop/portal/session/org_example_App/ics3");

    const auto createSession = [&](const QString &req, const QString &session) {
        QDBusReply<QVariantMap> create =
            inputCaptureIface.call(QStringLiteral("CreateSession"),
                                   req,
                                   appId,
                                   parent,
                                   QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                               {QStringLiteral("sessionHandle"), session}});
        QVERIFY(create.isValid());
        QVERIFY(create.value().value(QStringLiteral("ok")).toBool());
    };
    createSession(QStringLiteral("/request/ics-create-1"), s1);
    createSession(QStringLiteral("/request/ics-create-2"), s2);
    createSession(QStringLiteral("/request/ics-create-3"), s3);

    const auto setBarriers = [&](const QString &req, const QString &session) {
        QDBusReply<QVariantMap> call =
            inputCaptureIface.call(QStringLiteral("SetPointerBarriers"),
                                   req,
                                   appId,
                                   parent,
                                   session,
                                   QVariantMap{
                                       {QStringLiteral("barriers"),
                                        QVariantList{
                                            QVariantMap{
                                                {QStringLiteral("x"), 0},
                                                {QStringLiteral("y"), 0},
                                                {QStringLiteral("width"), 100},
                                                {QStringLiteral("height"), 100},
                                            },
                                        }},
                                   });
        QVERIFY(call.isValid());
        QVERIFY(call.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = unmarshal(call.value().value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("host_synced")).toBool(), true);
    };

    const auto enableSession = [&](const QString &req, const QString &session, bool allowPreempt) {
        QDBusReply<QVariantMap> call =
            inputCaptureIface.call(QStringLiteral("Enable"),
                                   req,
                                   appId,
                                   parent,
                                   session,
                                   QVariantMap{
                                       {QStringLiteral("initiatedByUserGesture"), true},
                                       {QStringLiteral("allow_preempt"), allowPreempt},
                                   });
        QVERIFY(call.isValid());
        QVERIFY(call.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = unmarshal(call.value().value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("enabled")).toBool(), true);
        QCOMPARE(results.value(QStringLiteral("host_synced")).toBool(), true);
    };

    const auto disableSession = [&](const QString &req, const QString &session) {
        QDBusReply<QVariantMap> call =
            inputCaptureIface.call(QStringLiteral("Disable"),
                                   req,
                                   appId,
                                   parent,
                                   session,
                                   QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
        QVERIFY(call.isValid());
        QVERIFY(call.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = unmarshal(call.value().value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("disabled")).toBool(), true);
        QCOMPARE(results.value(QStringLiteral("host_synced")).toBool(), true);
    };

    const auto releaseSession = [&](const QString &req, const QString &session) {
        QDBusReply<QVariantMap> call =
            inputCaptureIface.call(QStringLiteral("Release"),
                                   req,
                                   appId,
                                   parent,
                                   session,
                                   QVariantMap{{QStringLiteral("reason"), QStringLiteral("test-cleanup")}});
        QVERIFY(call.isValid());
        QVERIFY(call.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = unmarshal(call.value().value(QStringLiteral("results")));
        QCOMPARE(results.value(QStringLiteral("released")).toBool(), true);
        QCOMPARE(results.value(QStringLiteral("host_synced")).toBool(), true);
    };

    setBarriers(QStringLiteral("/request/ics-barrier-1"), s1);
    setBarriers(QStringLiteral("/request/ics-barrier-2"), s2);
    setBarriers(QStringLiteral("/request/ics-barrier-3"), s3);

    enableSession(QStringLiteral("/request/ics-enable-1"), s1, false);
    enableSession(QStringLiteral("/request/ics-enable-2"), s2, true);
    disableSession(QStringLiteral("/request/ics-disable-2"), s2);
    enableSession(QStringLiteral("/request/ics-enable-3"), s3, false);
    disableSession(QStringLiteral("/request/ics-disable-3"), s3);
    enableSession(QStringLiteral("/request/ics-enable-1b"), s1, false);

    releaseSession(QStringLiteral("/request/ics-release-1"), s1);
    releaseSession(QStringLiteral("/request/ics-release-2"), s2);
    releaseSession(QStringLiteral("/request/ics-release-3"), s3);
}

void ImplPortalServiceDbusTest::inputcapture_postRelease_disableDenied_contract()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakeInputCaptureService fakeInputCapture;
    if (!registerFakeInputCapture(fakeInputCapture)) {
        QSKIP("cannot register fake input capture service");
    }
    auto inputCaptureCleanup = qScopeGuard([&]() {
        unregisterFakeInputCapture();
    });

    PortalManager manager;
    Slm::PortalAdapter::PortalBackendService backend;
    ImplPortalService service(&manager, &backend);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register impl portal service name on session bus (likely already owned)");
    }

    QDBusInterface inputCaptureIface(QString::fromLatin1(kService),
                                     QString::fromLatin1(kPath),
                                     QString::fromLatin1(kIfaceInputCapture),
                                     bus);
    QVERIFY(inputCaptureIface.isValid());

    const QString appId = QStringLiteral("org.example.App");
    const QString parent = QStringLiteral("x11:1");
    const QString sessionPath =
        QStringLiteral("/org/desktop/portal/session/org_example_App/ic_post_release");

    QDBusReply<QVariantMap> create =
        inputCaptureIface.call(QStringLiteral("CreateSession"),
                               QStringLiteral("/request/icpr-create"),
                               appId,
                               parent,
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true},
                                           {QStringLiteral("sessionHandle"), sessionPath}});
    QVERIFY(create.isValid());
    QVERIFY(create.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> release =
        inputCaptureIface.call(QStringLiteral("Release"),
                               QStringLiteral("/request/icpr-release"),
                               appId,
                               parent,
                               sessionPath,
                               QVariantMap{{QStringLiteral("reason"), QStringLiteral("test")}});
    QVERIFY(release.isValid());
    QVERIFY(release.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> disable =
        inputCaptureIface.call(QStringLiteral("Disable"),
                               QStringLiteral("/request/icpr-disable"),
                               appId,
                               parent,
                               sessionPath,
                               QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(disable.isValid());
    QVERIFY(!disable.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(disable.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));
    QCOMPARE(disable.value().value(QStringLiteral("reason")).toString(),
             QStringLiteral("missing-session-path"));
}


QTEST_MAIN(ImplPortalServiceDbusTest)
#include "implportalservice_dbus_test.moc"
