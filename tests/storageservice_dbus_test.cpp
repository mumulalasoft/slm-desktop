#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/devicesd/devicesmanager.h"
#include "../src/daemon/devicesd/storageservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Storage";
constexpr const char kPath[] = "/org/slm/Desktop/Storage";
constexpr const char kIface[] = "org.slm.Desktop.Storage";
}

class StorageServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void ping_and_capabilities_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        DevicesManager manager;
        StorageService service(&manager);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> ping = iface.call(QStringLiteral("Ping"));
        QVERIFY(ping.isValid());
        QVERIFY(ping.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(ping.value().value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));

        QDBusReply<QVariantMap> caps = iface.call(QStringLiteral("GetCapabilities"));
        QVERIFY(caps.isValid());
        QVERIFY(caps.value().value(QStringLiteral("ok")).toBool());
        const QStringList list = caps.value().value(QStringLiteral("capabilities")).toStringList();
        QVERIFY(list.contains(QStringLiteral("storage-locations")));
        QVERIFY(list.contains(QStringLiteral("connect-server")));
        QVERIFY(list.contains(QStringLiteral("storage-policy-read")));
        QVERIFY(list.contains(QStringLiteral("storage-policy-write")));
    }

    void getStorageLocations_contractShape()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        DevicesManager manager;
        StorageService service(&manager);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetStorageLocations"));
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(out.contains(QStringLiteral("ok")));
        if (out.value(QStringLiteral("ok")).toBool()) {
            QVERIFY(out.value(QStringLiteral("rows")).canConvert<QVariantList>());
        }
    }

    void storagePolicy_methods_contractShape()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        DevicesManager manager;
        StorageService service(&manager);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> readReply = iface.call(QStringLiteral("StoragePolicyForPath"),
                                                       QStringLiteral("/definitely/not/a/storage/path"));
        QVERIFY(readReply.isValid());
        const QVariantMap readOut = readReply.value();
        QVERIFY(readOut.contains(QStringLiteral("ok")));
        if (!readOut.value(QStringLiteral("ok")).toBool()) {
            QVERIFY(readOut.contains(QStringLiteral("error")));
        }

        QVariantMap patch;
        patch.insert(QStringLiteral("automount"), true);
        QDBusReply<QVariantMap> writeReply = iface.call(QStringLiteral("SetStoragePolicyForPath"),
                                                        QStringLiteral("/definitely/not/a/storage/path"),
                                                        patch,
                                                        QStringLiteral("partition"));
        QVERIFY(writeReply.isValid());
        const QVariantMap writeOut = writeReply.value();
        QVERIFY(writeOut.contains(QStringLiteral("ok")));
        if (!writeOut.value(QStringLiteral("ok")).toBool()) {
            QVERIFY(writeOut.contains(QStringLiteral("error")));
        }

        QDBusReply<QVariantMap> locationsReply = iface.call(QStringLiteral("GetStorageLocations"));
        QVERIFY(locationsReply.isValid());
        const QVariantList rows = locationsReply.value().value(QStringLiteral("rows")).toList();
        if (rows.isEmpty()) {
            return;
        }

        const QVariantMap firstRow = rows.first().toMap();
        const QString firstPath = firstRow.value(QStringLiteral("path")).toString().trimmed();
        if (firstPath.isEmpty()) {
            return;
        }

        QDBusReply<QVariantMap> validReadReply = iface.call(QStringLiteral("StoragePolicyForPath"), firstPath);
        QVERIFY(validReadReply.isValid());
        const QVariantMap validReadOut = validReadReply.value();
        QVERIFY(validReadOut.value(QStringLiteral("ok")).toBool());
        QVERIFY(validReadOut.contains(QStringLiteral("policyInput")));
        QVERIFY(validReadOut.contains(QStringLiteral("policyOutput")));

        const QVariantMap policyInput = validReadOut.value(QStringLiteral("policyInput")).toMap();
        QVERIFY(policyInput.contains(QStringLiteral("uuid")));
        QVERIFY(policyInput.contains(QStringLiteral("partuuid")));
        QVERIFY(policyInput.contains(QStringLiteral("fstype")));
        QVERIFY(policyInput.contains(QStringLiteral("is_removable")));
        QVERIFY(policyInput.contains(QStringLiteral("is_system")));
        QVERIFY(policyInput.contains(QStringLiteral("is_encrypted")));

        const QVariantMap policyOutput = validReadOut.value(QStringLiteral("policyOutput")).toMap();
        QVERIFY(policyOutput.contains(QStringLiteral("action")));
        QVERIFY(policyOutput.contains(QStringLiteral("auto_open")));
        QVERIFY(policyOutput.contains(QStringLiteral("visible")));
        QVERIFY(policyOutput.contains(QStringLiteral("read_only")));
        QVERIFY(policyOutput.contains(QStringLiteral("exec")));
    }
};

QTEST_MAIN(StorageServiceDbusTest)
#include "storageservice_dbus_test.moc"
