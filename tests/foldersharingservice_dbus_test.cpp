#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/sharingd/sharingmanager.h"
#include "../src/daemon/sharingd/sharingservice.h"

namespace {
constexpr const char kService[] = "org.slm.Sharing";
constexpr const char kPath[]    = "/org/slm/Sharing";
constexpr const char kIface[]   = "org.slm.Sharing";
}

class SharingServiceCompatDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void pingAndCapabilities_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        SharingManager manager;
        manager.initialize();
        SharingService service(&manager);

        if (!bus.registerService(QString::fromLatin1(kService))) {
            QSKIP("cannot register org.slm.Sharing (likely already owned)");
        }
        bus.registerObject(QString::fromLatin1(kPath), &service,
                           QDBusConnection::ExportAllSlots
                               | QDBusConnection::ExportAllSignals
                               | QDBusConnection::ExportAllProperties);

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
        QVERIFY(pingReply.isValid());
        QVERIFY(pingReply.value().value(QStringLiteral("ok")).toBool());

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }

    void checkFileSharingEnvironment_contractShape()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        SharingManager manager;
        manager.initialize();
        SharingService service(&manager);

        if (!bus.registerService(QString::fromLatin1(kService))) {
            QSKIP("cannot register org.slm.Sharing (likely already owned)");
        }
        bus.registerObject(QString::fromLatin1(kPath), &service,
                           QDBusConnection::ExportAllSlots
                               | QDBusConnection::ExportAllSignals
                               | QDBusConnection::ExportAllProperties);

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QDBusReply<QVariantMap> envReply =
                iface.call(QStringLiteral("CheckFileSharingEnvironment"));
        QVERIFY(envReply.isValid());
        const QVariantMap env = envReply.value();
        QVERIFY(env.contains(QStringLiteral("ok")));
        QVERIFY(env.contains(QStringLiteral("ready")));
        QVERIFY(env.contains(QStringLiteral("issues")));
        QVERIFY(env.value(QStringLiteral("issues")).canConvert<QVariantList>());

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }

    void tryAutoFixFileSharing_contractShape()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        SharingManager manager;
        manager.initialize();
        SharingService service(&manager);

        if (!bus.registerService(QString::fromLatin1(kService))) {
            QSKIP("cannot register org.slm.Sharing (likely already owned)");
        }
        bus.registerObject(QString::fromLatin1(kPath), &service,
                           QDBusConnection::ExportAllSlots
                               | QDBusConnection::ExportAllSignals
                               | QDBusConnection::ExportAllProperties);

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QDBusReply<QVariantMap> fixReply =
                iface.call(QStringLiteral("TryAutoFixFileSharing"));
        QVERIFY(fixReply.isValid());
        const QVariantMap fix = fixReply.value();
        QVERIFY(fix.contains(QStringLiteral("ok")));
        QVERIFY(fix.contains(QStringLiteral("ready")));
        QVERIFY(fix.contains(QStringLiteral("issues")));
        QVERIFY(fix.contains(QStringLiteral("actions")));
        QVERIFY(fix.value(QStringLiteral("issues")).canConvert<QVariantList>());
        QVERIFY(fix.value(QStringLiteral("actions")).canConvert<QVariantList>());

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }

    void configureShare_invalidPathRejected()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        SharingManager manager;
        manager.initialize();
        SharingService service(&manager);

        if (!bus.registerService(QString::fromLatin1(kService))) {
            QSKIP("cannot register org.slm.Sharing (likely already owned)");
        }
        bus.registerObject(QString::fromLatin1(kPath), &service,
                           QDBusConnection::ExportAllSlots
                               | QDBusConnection::ExportAllSignals
                               | QDBusConnection::ExportAllProperties);

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QVariantMap options{
            {QStringLiteral("enabled"),   true},
            {QStringLiteral("shareName"), QStringLiteral("InvalidPathCase")}
        };
        const QDBusReply<QVariantMap> reply =
                iface.call(QStringLiteral("ConfigureShare"),
                           QStringLiteral("/definitely/not/an/existing/folder"),
                           QVariant::fromValue(options));
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(),
                 QStringLiteral("not-a-directory"));

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }
};

QTEST_MAIN(SharingServiceCompatDbusTest)
#include "foldersharingservice_dbus_test.moc"
