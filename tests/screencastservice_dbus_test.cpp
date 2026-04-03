#include <QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSignalSpy>

#include "../src/daemon/desktopd/screencastservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Screencast";
constexpr const char kPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kIface[] = "org.slm.Desktop.Screencast";
constexpr const char kPortalService[] = "org.slm.Desktop.Portal";
constexpr const char kPortalPath[] = "/org/slm/Desktop/Portal";
constexpr const char kPortalIface[] = "org.slm.Desktop.Portal";
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
        emit ScreencastSessionStateChanged(sessionHandle, QStringLiteral("org.example.App"), active, 1);
    }
    void EmitRevoked(const QString &sessionHandle, const QString &reason)
    {
        emit ScreencastSessionRevoked(sessionHandle, QStringLiteral("org.example.App"), reason, 0);
    }
};

class ScreencastServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void ping_and_state_transitions();
    void backend_env_pipewire_fallback();
    void sync_from_implportal_signal();
};

void ScreencastServiceDbusTest::ping_and_state_transitions()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    ScreencastService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register org.slm.Desktop.Screencast");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY2(iface.isValid(), qPrintable(bus.lastError().message()));

    QSignalSpy streamsSpy(&service, SIGNAL(SessionStreamsChanged(QString,QVariantList)));
    QSignalSpy endedSpy(&service, SIGNAL(SessionEnded(QString)));
    QSignalSpy revokedSpy(&service, SIGNAL(SessionRevoked(QString,QString)));

    QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
    QVERIFY(pingReply.isValid());
    QVERIFY(pingReply.value().value(QStringLiteral("ok")).toBool());
    QVERIFY(!pingReply.value().value(QStringLiteral("stream_backend")).toString().trimmed().isEmpty());

    QVariantList streams{
        QVariantMap{{QStringLiteral("node_id"), 51},
                    {QStringLiteral("source_type"), QStringLiteral("monitor")},
                    {QStringLiteral("cursor_mode"), QStringLiteral("embedded")}}
    };
    QDBusReply<QVariantMap> updateReply =
        iface.call(QStringLiteral("UpdateSessionStreams"),
                   QStringLiteral("/org/freedesktop/portal/desktop/session/1_100/slm0"),
                   streams);
    QVERIFY(updateReply.isValid());
    QVERIFY(updateReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(streamsSpy.count(), 1);

    QDBusReply<QVariantMap> sessionReply =
        iface.call(QStringLiteral("GetSessionStreams"),
                   QStringLiteral("/org/freedesktop/portal/desktop/session/1_100/slm0"));
    QVERIFY(sessionReply.isValid());
    QVERIFY(sessionReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(sessionReply.value().value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("streams")).toList().size(), 1);

    QDBusReply<QVariantMap> endReply =
        iface.call(QStringLiteral("EndSession"),
                   QStringLiteral("/org/freedesktop/portal/desktop/session/1_100/slm0"));
    QVERIFY(endReply.isValid());
    QVERIFY(endReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(endedSpy.count(), 1);

    QDBusReply<QVariantMap> revokeReply =
        iface.call(QStringLiteral("RevokeSession"),
                   QStringLiteral("/org/freedesktop/portal/desktop/session/1_100/slm1"),
                   QStringLiteral("policy-revoked"));
    QVERIFY(revokeReply.isValid());
    QVERIFY(revokeReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(revokedSpy.count(), 1);
}

void ScreencastServiceDbusTest::sync_from_implportal_signal()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    FakePortalService fakePortal;
    fakePortal.streams = QVariantList{
        QVariantMap{{QStringLiteral("node_id"), 7001},
                    {QStringLiteral("stream_id"), 901},
                    {QStringLiteral("source_type"), QStringLiteral("monitor")},
                    {QStringLiteral("cursor_mode"), QStringLiteral("embedded")}}
    };
    FakeImplPortalEmitter fakeImpl;

    QDBusConnectionInterface *iface = bus.interface();
    QVERIFY(iface);
    if (iface->isServiceRegistered(QString::fromLatin1(kPortalService)).value()) {
        bus.unregisterObject(QString::fromLatin1(kPortalPath));
        bus.unregisterService(QString::fromLatin1(kPortalService));
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kImplPortalService)).value()) {
        bus.unregisterObject(QString::fromLatin1(kImplPortalPath));
        bus.unregisterService(QString::fromLatin1(kImplPortalService));
    }
    if (!bus.registerService(QString::fromLatin1(kPortalService))) {
        QSKIP("cannot register fake org.slm.Desktop.Portal service");
    }
    if (!bus.registerObject(QString::fromLatin1(kPortalPath), &fakePortal, QDBusConnection::ExportAllSlots)) {
        QSKIP("cannot register fake org.slm.Desktop.Portal object");
    }
    if (!bus.registerService(QString::fromLatin1(kImplPortalService))) {
        QSKIP("cannot register fake impl portal service");
    }
    if (!bus.registerObject(QString::fromLatin1(kImplPortalPath),
                               &fakeImpl,
                               QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        QSKIP("cannot register fake impl portal object");
    }

    ScreencastService service;
    if (!service.serviceRegistered()) {
        bus.unregisterObject(QString::fromLatin1(kImplPortalPath));
        bus.unregisterService(QString::fromLatin1(kImplPortalService));
        bus.unregisterObject(QString::fromLatin1(kPortalPath));
        bus.unregisterService(QString::fromLatin1(kPortalService));
        QSKIP("cannot register org.slm.Desktop.Screencast");
    }

    QDBusInterface scIface(QString::fromLatin1(kService),
                           QString::fromLatin1(kPath),
                           QString::fromLatin1(kIface),
                           bus);
    QVERIFY(scIface.isValid());
    QDBusInterface implIface(QString::fromLatin1(kImplPortalService),
                             QString::fromLatin1(kImplPortalPath),
                             QString::fromLatin1(kImplPortalIface),
                             bus);
    QVERIFY(implIface.isValid());

    const QString session = QStringLiteral("/org/freedesktop/portal/desktop/session/2_200/slmA");
    QDBusReply<void> emitActive = implIface.call(QStringLiteral("EmitStateChanged"), session, true);
    QVERIFY(emitActive.isValid());
    QTest::qWait(10);

    QDBusReply<QVariantMap> streamsReply = scIface.call(QStringLiteral("GetSessionStreams"), session);
    QVERIFY(streamsReply.isValid());
    QVERIFY(streamsReply.value().value(QStringLiteral("ok")).toBool());
    const QVariantList streams = streamsReply.value().value(QStringLiteral("results")).toMap()
                                   .value(QStringLiteral("streams")).toList();
    QVERIFY(!streams.isEmpty());
    QCOMPARE(streams.constFirst().toMap().value(QStringLiteral("node_id")).toInt(), 7001);

    QDBusReply<void> emitRevoke =
        implIface.call(QStringLiteral("EmitRevoked"), session, QStringLiteral("policy"));
    QVERIFY(emitRevoke.isValid());
    QTest::qWait(10);
    QDBusReply<QVariantMap> afterRevoke = scIface.call(QStringLiteral("GetSessionStreams"), session);
    QVERIFY(afterRevoke.isValid());
    QCOMPARE(afterRevoke.value().value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("streams")).toList().size(), 0);

    bus.unregisterObject(QString::fromLatin1(kImplPortalPath));
    bus.unregisterService(QString::fromLatin1(kImplPortalService));
    bus.unregisterObject(QString::fromLatin1(kPortalPath));
    bus.unregisterService(QString::fromLatin1(kPortalService));
}

void ScreencastServiceDbusTest::backend_env_pipewire_fallback()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus unavailable");
    }

    const QByteArray prev = qgetenv("SLM_SCREENCAST_STREAM_BACKEND");
    qputenv("SLM_SCREENCAST_STREAM_BACKEND", QByteArrayLiteral("pipewire"));

    {
        ScreencastService service;
        if (!service.serviceRegistered()) {
            if (prev.isEmpty()) {
                qunsetenv("SLM_SCREENCAST_STREAM_BACKEND");
            } else {
                qputenv("SLM_SCREENCAST_STREAM_BACKEND", prev);
            }
            QSKIP("cannot register org.slm.Desktop.Screencast");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
        QVERIFY(pingReply.isValid());
        const QVariantMap ping = pingReply.value();
        QVERIFY(ping.value(QStringLiteral("ok")).toBool());
        QCOMPARE(ping.value(QStringLiteral("stream_backend_requested")).toString(),
                 QStringLiteral("pipewire"));
        const bool pwBuild = ping.value(QStringLiteral("pipewire_build_available")).toBool();
        if (pwBuild) {
            QCOMPARE(ping.value(QStringLiteral("stream_backend")).toString(),
                     QStringLiteral("pipewire"));
        } else {
            QCOMPARE(ping.value(QStringLiteral("stream_backend")).toString(),
                     QStringLiteral("portal-mirror"));
            QVERIFY(!ping.value(QStringLiteral("stream_backend_fallback_reason"))
                         .toString().trimmed().isEmpty());
        }
    }

    if (prev.isEmpty()) {
        qunsetenv("SLM_SCREENCAST_STREAM_BACKEND");
    } else {
        qputenv("SLM_SCREENCAST_STREAM_BACKEND", prev);
    }
}

QTEST_MAIN(ScreencastServiceDbusTest)
#include "screencastservice_dbus_test.moc"
