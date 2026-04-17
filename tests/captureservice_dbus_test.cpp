#include <QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/desktopd/captureservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Capture";
constexpr const char kPath[] = "/org/slm/Desktop/Capture";
constexpr const char kIface[] = "org.slm.Desktop.Capture";
constexpr const char kPortalService[] = "org.slm.Desktop.Portal";
constexpr const char kPortalPath[] = "/org/slm/Desktop/Portal";
constexpr const char kPortalIface[] = "org.slm.Desktop.Portal";
constexpr const char kScreencastService[] = "org.slm.Desktop.Screencast";
constexpr const char kScreencastPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kScreencastIface[] = "org.slm.Desktop.Screencast";
constexpr const char kImplPortalService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kImplPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kImplPortalIface[] = "org.freedesktop.impl.portal.desktop.slm";
}

class FakePortalService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Portal")

public:
    QVariantList streams;

public slots:
    QVariantMap GetScreencastSessionStreams(const QString &sessionPath)
    {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("response"), 0u},
            {QStringLiteral("results"),
             QVariantMap{
                 {QStringLiteral("session_handle"), sessionPath},
                 {QStringLiteral("streams"), streams},
             }},
        };
    }
};

class FakeImplPortalEmitter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.desktop.slm")

public:
    Q_SIGNAL void ScreencastSessionStateChanged(const QString &sessionHandle,
                                                const QString &appId,
                                                bool active,
                                                int activeCount);
    Q_SIGNAL void ScreencastSessionRevoked(const QString &sessionHandle,
                                           const QString &appId,
                                           const QString &reason,
                                           int activeCount);

public slots:
    void EmitStateChanged(const QString &sessionHandle, bool active)
    {
        emit ScreencastSessionStateChanged(sessionHandle, QStringLiteral("org.example.App"), active, 0);
    }
};

class FakeScreencastService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Screencast")

public:
    QVariantList streams;

public slots:
    QVariantMap GetSessionStreams(const QString &sessionPath)
    {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("response"), 0u},
            {QStringLiteral("results"),
             QVariantMap{
                 {QStringLiteral("session_handle"), sessionPath},
                 {QStringLiteral("streams"), streams},
             }},
        };
    }
};

class CaptureServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void pingAndCapabilities_contract();
    void screencastStreams_optionAndCache_contract();
    void screencastStreams_invalidInput_contract();
    void screencastSessionControls_contract();
    void screencastStreams_screencastSession_contract();
    void screencastStreams_portalSession_contract();
    void screencastCache_clearedByImplPortalSignal_contract();

private:
    QDBusConnection m_bus = QDBusConnection::sessionBus();
    bool registerFakePortal(FakePortalService &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kPortalService)).value()) {
            m_bus.unregisterObject(QString::fromLatin1(kPortalPath));
            m_bus.unregisterService(QString::fromLatin1(kPortalService));
        }
        if (!m_bus.registerService(QString::fromLatin1(kPortalService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kPortalPath), &fake, QDBusConnection::ExportAllSlots)) {
            m_bus.unregisterService(QString::fromLatin1(kPortalService));
            return false;
        }
        return true;
    }
    void unregisterFakePortal()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kPortalPath));
        m_bus.unregisterService(QString::fromLatin1(kPortalService));
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
    bool registerFakeImplPortalEmitter(FakeImplPortalEmitter &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kImplPortalService)).value()) {
            m_bus.unregisterObject(QString::fromLatin1(kImplPortalPath));
            m_bus.unregisterService(QString::fromLatin1(kImplPortalService));
        }
        if (!m_bus.registerService(QString::fromLatin1(kImplPortalService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kImplPortalPath), &fake, QDBusConnection::ExportAllSlots
                                                                   | QDBusConnection::ExportAllSignals)) {
            m_bus.unregisterService(QString::fromLatin1(kImplPortalService));
            return false;
        }
        return true;
    }
    void unregisterFakeImplPortalEmitter()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kImplPortalPath));
        m_bus.unregisterService(QString::fromLatin1(kImplPortalService));
    }
};

void CaptureServiceDbusTest::pingAndCapabilities_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    CaptureService service;
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

    QDBusReply<QVariantMap> capsReply = iface.call(QStringLiteral("GetCapabilities"));
    QVERIFY(capsReply.isValid());
    QVERIFY(capsReply.value().value(QStringLiteral("ok")).toBool());
    const QStringList caps = capsReply.value().value(QStringLiteral("capabilities")).toStringList();
    QVERIFY(caps.contains(QStringLiteral("screencast_streams")));
    QVERIFY(caps.contains(QStringLiteral("session_stream_upsert")));
    QVERIFY(caps.contains(QStringLiteral("session_clear")));
    QVERIFY(caps.contains(QStringLiteral("session_revoke")));
}

void CaptureServiceDbusTest::screencastStreams_optionAndCache_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    CaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    const QString session = QStringLiteral("/org/desktop/portal/session/org_example_App/cache1");
    QVariantMap opts;
    opts.insert(QStringLiteral("node_id"), 6001);
    opts.insert(QStringLiteral("stream_id"), 77);
    opts.insert(QStringLiteral("source_type"), QStringLiteral("window"));
    opts.insert(QStringLiteral("cursor_mode"), QStringLiteral("metadata"));
    QDBusReply<QVariantMap> firstReply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   session,
                   QStringLiteral("org.example.App"),
                   opts);
    QVERIFY(firstReply.isValid());
    const QVariantMap firstOut = firstReply.value();
    QVERIFY(firstOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap firstResults = firstOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(firstResults.value(QStringLiteral("stream_provider")).toString(), QStringLiteral("options"));
    const QVariantList firstStreams = firstResults.value(QStringLiteral("streams")).toList();
    QVERIFY(!firstStreams.isEmpty());
    const QVariantMap firstStream = firstStreams.constFirst().toMap();
    QCOMPARE(firstStream.value(QStringLiteral("node_id")).toInt(), 6001);
    QCOMPARE(firstStream.value(QStringLiteral("stream_id")).toInt(), 77);

    QDBusReply<QVariantMap> cachedReply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   session,
                   QStringLiteral("org.example.App"),
                   QVariantMap{});
    QVERIFY(cachedReply.isValid());
    const QVariantMap cachedOut = cachedReply.value();
    QVERIFY(cachedOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap cachedResults = cachedOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(cachedResults.value(QStringLiteral("stream_provider")).toString(), QStringLiteral("cache"));
    const QVariantList cachedStreams = cachedResults.value(QStringLiteral("streams")).toList();
    QVERIFY(!cachedStreams.isEmpty());
    QCOMPARE(cachedStreams.constFirst().toMap().value(QStringLiteral("node_id")).toInt(), 6001);

    // tuple style (pipewire-like): streams = [[node_id, {props...}]]
    const QString tupleSession = QStringLiteral("/org/desktop/portal/session/org_example_App/tuple1");
    QVariantMap tupleOpts;
    tupleOpts.insert(QStringLiteral("streams"),
                     QVariantList{
                         QVariantList{
                             8123,
                             QVariantMap{
                                 {QStringLiteral("stream_id"), 501},
                                 {QStringLiteral("source_type"), QStringLiteral("window")},
                                 {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
                             },
                         },
                     });
    QDBusReply<QVariantMap> tupleReply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   tupleSession,
                   QStringLiteral("org.example.App"),
                   tupleOpts);
    QVERIFY(tupleReply.isValid());
    const QVariantMap tupleOut = tupleReply.value();
    QVERIFY(tupleOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap tupleResults = tupleOut.value(QStringLiteral("results")).toMap();
    const QVariantMap tupleStream = tupleResults.value(QStringLiteral("streams")).toList().constFirst().toMap();
    QCOMPARE(tupleStream.value(QStringLiteral("node_id")).toInt(), 8123);
    QCOMPARE(tupleStream.value(QStringLiteral("stream_id")).toInt(), 501);
}

void CaptureServiceDbusTest::screencastStreams_invalidInput_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    CaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    QDBusReply<QVariantMap> reply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   QStringLiteral(""),
                   QStringLiteral("org.example.App"),
                   QVariantMap{});
    QVERIFY(reply.isValid());
    const QVariantMap out = reply.value();
    QVERIFY(!out.value(QStringLiteral("ok")).toBool());
    QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
    QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 3u);
}

void CaptureServiceDbusTest::screencastSessionControls_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    CaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    const QString session = QStringLiteral("/org/desktop/portal/session/org_example_App/ctrl1");

    const QVariantList streams{
        QVariantMap{
            {QStringLiteral("stream_id"), 9001},
            {QStringLiteral("pipewire_node_id"), 4242},
            {QStringLiteral("source_type"), QStringLiteral("window")},
            {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
        },
    };
    QDBusReply<QVariantMap> setReply =
        iface.call(QStringLiteral("SetScreencastSessionStreams"),
                   session,
                   streams,
                   QVariantMap{});
    QVERIFY(setReply.isValid());
    const QVariantMap setOut = setReply.value();
    QVERIFY(setOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap setResults = setOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(setResults.value(QStringLiteral("stream_provider")).toString(),
             QStringLiteral("session-upsert"));
    QCOMPARE(setResults.value(QStringLiteral("stream_count")).toInt(), 1);
    QCOMPARE(setResults.value(QStringLiteral("streams")).toList().constFirst().toMap()
                 .value(QStringLiteral("node_id")).toInt(),
             4242);

    QDBusReply<QVariantMap> getReply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   session,
                   QStringLiteral("org.example.App"),
                   QVariantMap{});
    QVERIFY(getReply.isValid());
    const QVariantMap getOut = getReply.value();
    QVERIFY(getOut.value(QStringLiteral("ok")).toBool());
    QCOMPARE(getOut.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("stream_provider")).toString(),
             QStringLiteral("cache"));

    QDBusReply<QVariantMap> clearReply =
        iface.call(QStringLiteral("ClearScreencastSession"), session);
    QVERIFY(clearReply.isValid());
    const QVariantMap clearOut = clearReply.value();
    QVERIFY(clearOut.value(QStringLiteral("ok")).toBool());
    QCOMPARE(clearOut.value(QStringLiteral("results")).toMap().value(QStringLiteral("cleared")).toBool(), true);
    QCOMPARE(clearOut.value(QStringLiteral("results")).toMap().value(QStringLiteral("had_streams")).toBool(), true);

    QDBusReply<QVariantMap> revokeReply =
        iface.call(QStringLiteral("RevokeScreencastSession"),
                   session,
                   QStringLiteral("policy-revoke"));
    QVERIFY(revokeReply.isValid());
    const QVariantMap revokeOut = revokeReply.value();
    QVERIFY(revokeOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap revokeResults = revokeOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(revokeResults.value(QStringLiteral("revoked")).toBool(), true);
    QCOMPARE(revokeResults.value(QStringLiteral("reason")).toString(), QStringLiteral("policy-revoke"));
}

void CaptureServiceDbusTest::screencastStreams_screencastSession_contract()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    FakeScreencastService fakeScreencast;
    fakeScreencast.streams = QVariantList{
        QVariantMap{
            {QStringLiteral("node_id"), 9001},
            {QStringLiteral("source_type"), QStringLiteral("window")},
            {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
        }};
    if (!registerFakeScreencast(fakeScreencast)) {
        QSKIP("cannot register fake org.slm.Desktop.Screencast");
    }

    CaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    const QString session = QStringLiteral("/org/desktop/portal/session/org_example_App/screencast1");
    QDBusReply<QVariantMap> reply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   session,
                   QStringLiteral("org.example.App"),
                   QVariantMap{});
    QVERIFY(reply.isValid());
    const QVariantMap out = reply.value();
    QVERIFY(out.value(QStringLiteral("ok")).toBool());
    const QVariantMap results = out.value(QStringLiteral("results")).toMap();
    QCOMPARE(results.value(QStringLiteral("stream_provider")).toString(),
             QStringLiteral("screencast-session"));
    const QVariantList streams = results.value(QStringLiteral("streams")).toList();
    QVERIFY(!streams.isEmpty());
    QCOMPARE(streams.constFirst().toMap().value(QStringLiteral("node_id")).toInt(), 9001);

    unregisterFakeScreencast();
}

void CaptureServiceDbusTest::screencastStreams_portalSession_contract()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    FakePortalService fakePortal;
    fakePortal.streams = {
        QVariantList{
            9900,
            QVariantMap{
                {QStringLiteral("stream_id"), 321},
                {QStringLiteral("source_type"), QStringLiteral("window")},
                {QStringLiteral("cursor_mode"), QStringLiteral("metadata")},
            },
        },
    };
    if (!registerFakePortal(fakePortal)) {
        QSKIP("cannot register fake org.slm.Desktop.Portal service");
    }

    CaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         m_bus);
    QVERIFY(iface.isValid());

    const QString session = QStringLiteral("/org/desktop/portal/session/org_example_App/portal1");
    QDBusReply<QVariantMap> reply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   session,
                   QStringLiteral("org.example.App"),
                   QVariantMap{});
    QVERIFY(reply.isValid());
    const QVariantMap out = reply.value();
    QVERIFY(out.value(QStringLiteral("ok")).toBool());
    const QVariantMap results = out.value(QStringLiteral("results")).toMap();
    QCOMPARE(results.value(QStringLiteral("stream_provider")).toString(), QStringLiteral("portal-session"));
    const QVariantMap stream = results.value(QStringLiteral("streams")).toList().constFirst().toMap();
    QCOMPARE(stream.value(QStringLiteral("node_id")).toInt(), 9900);
    QCOMPARE(stream.value(QStringLiteral("stream_id")).toInt(), 321);

    unregisterFakePortal();
}

void CaptureServiceDbusTest::screencastCache_clearedByImplPortalSignal_contract()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    FakeImplPortalEmitter emitter;
    if (!registerFakeImplPortalEmitter(emitter)) {
        QSKIP("cannot register fake impl portal emitter");
    }

    CaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         m_bus);
    QVERIFY(iface.isValid());

    const QString session = QStringLiteral("/org/desktop/portal/session/org_example_App/signal1");
    QVariantMap opts{
        {QStringLiteral("stream_id"), 1234},
        {QStringLiteral("node_id"), 9876},
    };
    QDBusReply<QVariantMap> firstReply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   session,
                   QStringLiteral("org.example.App"),
                   opts);
    QVERIFY(firstReply.isValid());
    QVERIFY(firstReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(firstReply.value().value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("stream_provider")).toString(),
             QStringLiteral("options"));

    emitter.EmitStateChanged(session, false);

    QDBusReply<QVariantMap> afterReply =
        iface.call(QStringLiteral("GetScreencastStreams"),
                   session,
                   QStringLiteral("org.example.App"),
                   QVariantMap{});
    QVERIFY(afterReply.isValid());
    const QVariantMap afterOut = afterReply.value();
    QVERIFY(afterOut.value(QStringLiteral("ok")).toBool());
    const QVariantMap afterResults = afterOut.value(QStringLiteral("results")).toMap();
    QCOMPARE(afterResults.value(QStringLiteral("stream_provider")).toString(), QStringLiteral("none"));
    QVERIFY(afterResults.value(QStringLiteral("streams")).toList().isEmpty());

    unregisterFakeImplPortalEmitter();
}

QTEST_MAIN(CaptureServiceDbusTest)
#include "captureservice_dbus_test.moc"
