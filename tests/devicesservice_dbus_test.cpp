#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/devicesd/devicesmanager.h"
#include "../src/daemon/devicesd/devicesservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Devices";
constexpr const char kPath[] = "/org/slm/Desktop/Devices";
constexpr const char kIface[] = "org.slm.Desktop.Devices";
}

class DevicesServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void ping_returnsOkMap()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        DevicesManager manager;
        DevicesService service(&manager);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Ping"));
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));
        QCOMPARE(out.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));

        QDBusReply<QVariantMap> capsReply = iface.call(QStringLiteral("GetCapabilities"));
        QVERIFY(capsReply.isValid());
        const QVariantMap caps = capsReply.value();
        QVERIFY(caps.value(QStringLiteral("ok")).toBool());
        QCOMPARE(caps.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));
        const QStringList list = caps.value(QStringLiteral("capabilities")).toStringList();
        QVERIFY(list.contains(QStringLiteral("mount")));
        QVERIFY(list.contains(QStringLiteral("format")));
    }

    void invalidMountTarget_isRejected()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        DevicesManager manager;
        DevicesService service(&manager);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Mount"), QString());
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-target"));
    }

    void invalidEjectTarget_isRejected()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        DevicesManager manager;
        DevicesService service(&manager);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Eject"), QString());
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-target"));
    }
};

QTEST_MAIN(DevicesServiceDbusTest)
#include "devicesservice_dbus_test.moc"
