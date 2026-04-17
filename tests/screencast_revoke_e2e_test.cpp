#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/desktopd/captureservice.h"
#include "../src/daemon/desktopd/screencastservice.h"
#include "../src/daemon/portald/implportalservice.h"
#include "../src/daemon/portald/portalmanager.h"

namespace {
constexpr const char kImplService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kImplPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kImplIface[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kImplScreenCastIface[] = "org.freedesktop.impl.portal.ScreenCast";
constexpr const char kCaptureService[] = "org.slm.Desktop.Capture";
constexpr const char kCapturePath[] = "/org/slm/Desktop/Capture";
constexpr const char kCaptureIface[] = "org.slm.Desktop.Capture";
constexpr const char kScreencastService[] = "org.slm.Desktop.Screencast";
constexpr const char kScreencastPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kScreencastIface[] = "org.slm.Desktop.Screencast";
}

class ScreencastRevokeE2eTest : public QObject
{
    Q_OBJECT

private slots:
    void revoke_clears_cross_service_state();
    void close_clears_cross_service_state();
    void close_all_clears_multi_session_state();
    void pipewire_backend_session_smoke();
};

void ScreencastRevokeE2eTest::revoke_clears_cross_service_state()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    CaptureService captureService;
    ScreencastService screencastService;
    if (!captureService.serviceRegistered() || !screencastService.serviceRegistered()) {
        QSKIP("capture/screencast DBus services already owned");
    }

    PortalManager manager;
    ImplPortalService implService(&manager, nullptr);
    if (!implService.serviceRegistered()) {
        QSKIP("impl portal service already owned");
    }

    QDBusInterface implRoot(QString::fromLatin1(kImplService),
                            QString::fromLatin1(kImplPath),
                            QString::fromLatin1(kImplIface),
                            bus);
    QDBusInterface implScreencast(QString::fromLatin1(kImplService),
                                  QString::fromLatin1(kImplPath),
                                  QString::fromLatin1(kImplScreenCastIface),
                                  bus);
    QDBusInterface captureIface(QString::fromLatin1(kCaptureService),
                                QString::fromLatin1(kCapturePath),
                                QString::fromLatin1(kCaptureIface),
                                bus);
    QDBusInterface scIface(QString::fromLatin1(kScreencastService),
                           QString::fromLatin1(kScreencastPath),
                           QString::fromLatin1(kScreencastIface),
                           bus);
    QVERIFY(implRoot.isValid());
    QVERIFY(implScreencast.isValid());
    QVERIFY(captureIface.isValid());
    QVERIFY(scIface.isValid());

    const QString session = QStringLiteral("/org/freedesktop/portal/desktop/session/3_300/e2e0");

    QDBusReply<QVariantMap> createReply =
        implScreencast.call(QStringLiteral("CreateSession"),
                            QStringLiteral("/request/e2e-create"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            QVariantMap{{QStringLiteral("sessionPath"), session},
                                        {QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(createReply.isValid());
    QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

    const QVariantList streams{
        QVariantMap{
            {QStringLiteral("node_id"), 9911},
            {QStringLiteral("stream_id"), 71},
            {QStringLiteral("source_type"), QStringLiteral("window")},
            {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
        }
    };
    {
        QDBusReply<QVariantMap> seedSc =
            scIface.call(QStringLiteral("UpdateSessionStreams"), session, streams);
        QVERIFY(seedSc.isValid());
        QVERIFY(seedSc.value().value(QStringLiteral("ok")).toBool());
    }
    {
        QDBusReply<QVariantMap> seedCapture =
            captureIface.call(QStringLiteral("SetScreencastSessionStreams"),
                              session,
                              streams,
                              QVariantMap{});
        QVERIFY(seedCapture.isValid());
        QVERIFY(seedCapture.value().value(QStringLiteral("ok")).toBool());
    }

    {
        QDBusReply<QVariantMap> before =
            captureIface.call(QStringLiteral("GetScreencastStreams"),
                              session,
                              QStringLiteral("org.example.App"),
                              QVariantMap{});
        QVERIFY(before.isValid());
        QVERIFY(before.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(before.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("stream_provider")).toString(),
                 QStringLiteral("cache"));
    }

    QDBusReply<QVariantMap> revokeReply =
        implRoot.call(QStringLiteral("RevokeScreencastSession"),
                      session,
                      QStringLiteral("policy-e2e"));
    QVERIFY(revokeReply.isValid());
    QVERIFY(revokeReply.value().value(QStringLiteral("ok")).toBool());
    QTest::qWait(20);

    {
        QDBusReply<QVariantMap> stateReply = implRoot.call(QStringLiteral("GetScreencastState"));
        QVERIFY(stateReply.isValid());
        QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = stateReply.value().value(QStringLiteral("results")).toMap();
        QCOMPARE(results.value(QStringLiteral("active_count")).toInt(), 0);
        QCOMPARE(results.value(QStringLiteral("active")).toBool(), false);
    }
    {
        QDBusReply<QVariantMap> scStreams = scIface.call(QStringLiteral("GetSessionStreams"), session);
        QVERIFY(scStreams.isValid());
        QVERIFY(scStreams.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(scStreams.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("streams")).toList().size(),
                 0);
    }
    {
        QDBusReply<QVariantMap> after =
            captureIface.call(QStringLiteral("GetScreencastStreams"),
                              session,
                              QStringLiteral("org.example.App"),
                              QVariantMap{});
        QVERIFY(after.isValid());
        QVERIFY(after.value().value(QStringLiteral("ok")).toBool());
        const QString provider = after.value().value(QStringLiteral("results")).toMap()
                                     .value(QStringLiteral("stream_provider")).toString();
        QVERIFY2(provider != QStringLiteral("cache"),
                 qPrintable(QStringLiteral("expected non-cache provider after revoke, got %1")
                                .arg(provider)));
    }
}

void ScreencastRevokeE2eTest::close_clears_cross_service_state()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    CaptureService captureService;
    ScreencastService screencastService;
    if (!captureService.serviceRegistered() || !screencastService.serviceRegistered()) {
        QSKIP("capture/screencast DBus services already owned");
    }

    PortalManager manager;
    ImplPortalService implService(&manager, nullptr);
    if (!implService.serviceRegistered()) {
        QSKIP("impl portal service already owned");
    }

    QDBusInterface implRoot(QString::fromLatin1(kImplService),
                            QString::fromLatin1(kImplPath),
                            QString::fromLatin1(kImplIface),
                            bus);
    QDBusInterface implScreencast(QString::fromLatin1(kImplService),
                                  QString::fromLatin1(kImplPath),
                                  QString::fromLatin1(kImplScreenCastIface),
                                  bus);
    QDBusInterface captureIface(QString::fromLatin1(kCaptureService),
                                QString::fromLatin1(kCapturePath),
                                QString::fromLatin1(kCaptureIface),
                                bus);
    QDBusInterface scIface(QString::fromLatin1(kScreencastService),
                           QString::fromLatin1(kScreencastPath),
                           QString::fromLatin1(kScreencastIface),
                           bus);
    QVERIFY(implRoot.isValid());
    QVERIFY(implScreencast.isValid());
    QVERIFY(captureIface.isValid());
    QVERIFY(scIface.isValid());

    const QString session = QStringLiteral("/org/freedesktop/portal/desktop/session/3_300/e2e-close0");

    QDBusReply<QVariantMap> createReply =
        implScreencast.call(QStringLiteral("CreateSession"),
                            QStringLiteral("/request/e2e-close-create"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            QVariantMap{{QStringLiteral("sessionPath"), session},
                                        {QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(createReply.isValid());
    QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

    const QVariantList streams{
        QVariantMap{
            {QStringLiteral("node_id"), 9922},
            {QStringLiteral("stream_id"), 72},
            {QStringLiteral("source_type"), QStringLiteral("window")},
            {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
        }
    };
    {
        QDBusReply<QVariantMap> seedSc =
            scIface.call(QStringLiteral("UpdateSessionStreams"), session, streams);
        QVERIFY(seedSc.isValid());
        QVERIFY(seedSc.value().value(QStringLiteral("ok")).toBool());
    }
    {
        QDBusReply<QVariantMap> seedCapture =
            captureIface.call(QStringLiteral("SetScreencastSessionStreams"),
                              session,
                              streams,
                              QVariantMap{});
        QVERIFY(seedCapture.isValid());
        QVERIFY(seedCapture.value().value(QStringLiteral("ok")).toBool());
    }

    {
        QDBusReply<QVariantMap> before =
            captureIface.call(QStringLiteral("GetScreencastStreams"),
                              session,
                              QStringLiteral("org.example.App"),
                              QVariantMap{});
        QVERIFY(before.isValid());
        QVERIFY(before.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(before.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("stream_provider")).toString(),
                 QStringLiteral("cache"));
    }

    QDBusReply<QVariantMap> closeReply =
        implRoot.call(QStringLiteral("CloseScreencastSession"), session);
    QVERIFY(closeReply.isValid());
    QVERIFY(closeReply.value().value(QStringLiteral("ok")).toBool());
    QTest::qWait(20);

    {
        QDBusReply<QVariantMap> stateReply = implRoot.call(QStringLiteral("GetScreencastState"));
        QVERIFY(stateReply.isValid());
        QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = stateReply.value().value(QStringLiteral("results")).toMap();
        QCOMPARE(results.value(QStringLiteral("active_count")).toInt(), 0);
        QCOMPARE(results.value(QStringLiteral("active")).toBool(), false);
    }
    {
        QDBusReply<QVariantMap> scStreams = scIface.call(QStringLiteral("GetSessionStreams"), session);
        QVERIFY(scStreams.isValid());
        QVERIFY(scStreams.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(scStreams.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("streams")).toList().size(),
                 0);
    }
    {
        QDBusReply<QVariantMap> after =
            captureIface.call(QStringLiteral("GetScreencastStreams"),
                              session,
                              QStringLiteral("org.example.App"),
                              QVariantMap{});
        QVERIFY(after.isValid());
        QVERIFY(after.value().value(QStringLiteral("ok")).toBool());
        const QString provider = after.value().value(QStringLiteral("results")).toMap()
                                     .value(QStringLiteral("stream_provider")).toString();
        QVERIFY2(provider != QStringLiteral("cache"),
                 qPrintable(QStringLiteral("expected non-cache provider after close, got %1")
                                .arg(provider)));
    }
}

void ScreencastRevokeE2eTest::close_all_clears_multi_session_state()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    CaptureService captureService;
    ScreencastService screencastService;
    if (!captureService.serviceRegistered() || !screencastService.serviceRegistered()) {
        QSKIP("capture/screencast DBus services already owned");
    }

    PortalManager manager;
    ImplPortalService implService(&manager, nullptr);
    if (!implService.serviceRegistered()) {
        QSKIP("impl portal service already owned");
    }

    QDBusInterface implRoot(QString::fromLatin1(kImplService),
                            QString::fromLatin1(kImplPath),
                            QString::fromLatin1(kImplIface),
                            bus);
    QDBusInterface implScreencast(QString::fromLatin1(kImplService),
                                  QString::fromLatin1(kImplPath),
                                  QString::fromLatin1(kImplScreenCastIface),
                                  bus);
    QDBusInterface captureIface(QString::fromLatin1(kCaptureService),
                                QString::fromLatin1(kCapturePath),
                                QString::fromLatin1(kCaptureIface),
                                bus);
    QDBusInterface scIface(QString::fromLatin1(kScreencastService),
                           QString::fromLatin1(kScreencastPath),
                           QString::fromLatin1(kScreencastIface),
                           bus);
    QVERIFY(implRoot.isValid());
    QVERIFY(implScreencast.isValid());
    QVERIFY(captureIface.isValid());
    QVERIFY(scIface.isValid());

    const QStringList sessions{
        QStringLiteral("/org/freedesktop/portal/desktop/session/3_300/e2e-close-all0"),
        QStringLiteral("/org/freedesktop/portal/desktop/session/3_300/e2e-close-all1"),
    };

    for (int i = 0; i < sessions.size(); ++i) {
        const QString &session = sessions.at(i);
        QDBusReply<QVariantMap> createReply =
            implScreencast.call(QStringLiteral("CreateSession"),
                                QStringLiteral("/request/e2e-close-all-create-%1").arg(i),
                                QStringLiteral("org.example.App"),
                                QStringLiteral("x11:1"),
                                QVariantMap{{QStringLiteral("sessionPath"), session},
                                            {QStringLiteral("initiatedByUserGesture"), true}});
        QVERIFY(createReply.isValid());
        QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

        const QVariantList streams{
            QVariantMap{
                {QStringLiteral("node_id"), 9930 + i},
                {QStringLiteral("stream_id"), 80 + i},
                {QStringLiteral("source_type"), QStringLiteral("window")},
                {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
            }
        };
        {
            QDBusReply<QVariantMap> seedSc =
                scIface.call(QStringLiteral("UpdateSessionStreams"), session, streams);
            QVERIFY(seedSc.isValid());
            QVERIFY(seedSc.value().value(QStringLiteral("ok")).toBool());
        }
        {
            QDBusReply<QVariantMap> seedCapture =
                captureIface.call(QStringLiteral("SetScreencastSessionStreams"),
                                  session,
                                  streams,
                                  QVariantMap{});
            QVERIFY(seedCapture.isValid());
            QVERIFY(seedCapture.value().value(QStringLiteral("ok")).toBool());
        }

        QDBusReply<QVariantMap> before =
            captureIface.call(QStringLiteral("GetScreencastStreams"),
                              session,
                              QStringLiteral("org.example.App"),
                              QVariantMap{});
        QVERIFY(before.isValid());
        QVERIFY(before.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(before.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("stream_provider")).toString(),
                 QStringLiteral("cache"));
    }

    QDBusReply<QVariantMap> closeAllReply =
        implRoot.call(QStringLiteral("CloseAllScreencastSessions"));
    QVERIFY(closeAllReply.isValid());
    QVERIFY(closeAllReply.value().value(QStringLiteral("ok")).toBool());
    QTest::qWait(30);

    {
        QDBusReply<QVariantMap> stateReply = implRoot.call(QStringLiteral("GetScreencastState"));
        QVERIFY(stateReply.isValid());
        QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap results = stateReply.value().value(QStringLiteral("results")).toMap();
        QCOMPARE(results.value(QStringLiteral("active_count")).toInt(), 0);
        QCOMPARE(results.value(QStringLiteral("active")).toBool(), false);
    }

    for (const QString &session : sessions) {
        QDBusReply<QVariantMap> scStreams = scIface.call(QStringLiteral("GetSessionStreams"), session);
        QVERIFY(scStreams.isValid());
        QVERIFY(scStreams.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(scStreams.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("streams")).toList().size(),
                 0);

        QDBusReply<QVariantMap> after =
            captureIface.call(QStringLiteral("GetScreencastStreams"),
                              session,
                              QStringLiteral("org.example.App"),
                              QVariantMap{});
        QVERIFY(after.isValid());
        QVERIFY(after.value().value(QStringLiteral("ok")).toBool());
        const QString provider = after.value().value(QStringLiteral("results")).toMap()
                                     .value(QStringLiteral("stream_provider")).toString();
        QVERIFY2(provider != QStringLiteral("cache"),
                 qPrintable(QStringLiteral("expected non-cache provider after close-all for %1, got %2")
                                .arg(session, provider)));
    }
}

void ScreencastRevokeE2eTest::pipewire_backend_session_smoke()
{
#if !defined(SLM_HAVE_PIPEWIRE)
    QSKIP("PipeWire build support is disabled");
#else
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    qputenv("SLM_SCREENCAST_STREAM_BACKEND", QByteArrayLiteral("pipewire"));

    CaptureService captureService;
    ScreencastService screencastService;
    if (!captureService.serviceRegistered() || !screencastService.serviceRegistered()) {
        qunsetenv("SLM_SCREENCAST_STREAM_BACKEND");
        QSKIP("capture/screencast DBus services already owned");
    }

    QDBusReply<QVariantMap> pingReply =
        QDBusInterface(QString::fromLatin1(kScreencastService),
                       QString::fromLatin1(kScreencastPath),
                       QString::fromLatin1(kScreencastIface),
                       bus)
            .call(QStringLiteral("Ping"));
    if (!pingReply.isValid()) {
        qunsetenv("SLM_SCREENCAST_STREAM_BACKEND");
        QSKIP("cannot query screencast backend state");
    }
    const QVariantMap ping = pingReply.value();
    const QString activeBackend = ping.value(QStringLiteral("stream_backend")).toString();
    if (activeBackend != QStringLiteral("pipewire")) {
        const QString fallbackReason =
            ping.value(QStringLiteral("stream_backend_fallback_reason")).toString();
        qunsetenv("SLM_SCREENCAST_STREAM_BACKEND");
        QSKIP(qPrintable(QStringLiteral("pipewire backend not active (%1)")
                             .arg(fallbackReason.isEmpty() ? QStringLiteral("no-reason")
                                                           : fallbackReason)));
    }

    PortalManager manager;
    ImplPortalService implService(&manager, nullptr);
    if (!implService.serviceRegistered()) {
        qunsetenv("SLM_SCREENCAST_STREAM_BACKEND");
        QSKIP("impl portal service already owned");
    }

    QDBusInterface implRoot(QString::fromLatin1(kImplService),
                            QString::fromLatin1(kImplPath),
                            QString::fromLatin1(kImplIface),
                            bus);
    QDBusInterface implScreencast(QString::fromLatin1(kImplService),
                                  QString::fromLatin1(kImplPath),
                                  QString::fromLatin1(kImplScreenCastIface),
                                  bus);
    QVERIFY(implRoot.isValid());
    QVERIFY(implScreencast.isValid());

    const QString session =
        QStringLiteral("/org/freedesktop/portal/desktop/session/3_300/e2e-pw0");
    QDBusReply<QVariantMap> createReply =
        implScreencast.call(QStringLiteral("CreateSession"),
                            QStringLiteral("/request/e2e-pw-create"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            QVariantMap{{QStringLiteral("sessionPath"), session},
                                        {QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(createReply.isValid());
    QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> selectReply =
        implScreencast.call(QStringLiteral("SelectSources"),
                            QStringLiteral("/request/e2e-pw-select"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            session,
                            QVariantMap{{QStringLiteral("sources"),
                                         QVariantList{QStringLiteral("monitor")}},
                                        {QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(selectReply.isValid());
    QVERIFY(selectReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> startReply =
        implScreencast.call(QStringLiteral("Start"),
                            QStringLiteral("/request/e2e-pw-start"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            session,
                            QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(startReply.isValid());
    QVERIFY(startReply.value().value(QStringLiteral("ok")).toBool());
    QVERIFY(startReply.value().value(QStringLiteral("results")).toMap()
                .value(QStringLiteral("streams")).toList().size() > 0);

    {
        QDBusReply<QVariantMap> stateReply = implRoot.call(QStringLiteral("GetScreencastState"));
        QVERIFY(stateReply.isValid());
        QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(stateReply.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("active_count")).toInt(),
                 1);
    }

    QDBusReply<QVariantMap> stopReply =
        implScreencast.call(QStringLiteral("Stop"),
                            QStringLiteral("/request/e2e-pw-stop"),
                            QStringLiteral("org.example.App"),
                            QStringLiteral("x11:1"),
                            session,
                            QVariantMap{{QStringLiteral("closeSession"), true},
                                        {QStringLiteral("initiatedByUserGesture"), true}});
    QVERIFY(stopReply.isValid());
    QVERIFY(stopReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(stopReply.value().value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("session_closed")).toBool(),
             true);

    {
        QDBusReply<QVariantMap> stateReply = implRoot.call(QStringLiteral("GetScreencastState"));
        QVERIFY(stateReply.isValid());
        QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(stateReply.value().value(QStringLiteral("results")).toMap()
                     .value(QStringLiteral("active_count")).toInt(),
                 0);
    }

    qunsetenv("SLM_SCREENCAST_STREAM_BACKEND");
#endif
}

QTEST_MAIN(ScreencastRevokeE2eTest)
#include "screencast_revoke_e2e_test.moc"
