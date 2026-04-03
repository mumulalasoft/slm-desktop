#include <QtTest/QtTest>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/desktopd/slmcapabilityservice.h"

namespace {
constexpr const char kService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kPath[] = "/org/freedesktop/SLMCapabilities";
constexpr const char kIface[] = "org.freedesktop.SLMCapabilities";
}

class SlmCapabilityServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void denyFileActionWithoutGesture();
    void denyAdminPermissionEndpointsForThirdPartyCaller();
};

void SlmCapabilityServiceDbusTest::denyFileActionWithoutGesture()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    SlmCapabilityService service(nullptr);
    QVERIFY(service.serviceRegistered());

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    const QVariantMap listContext{
        {QStringLiteral("uris"), QStringList{QStringLiteral("file:///tmp/test.txt")}},
        {QStringLiteral("selection_count"), 1},
        {QStringLiteral("target"), QStringLiteral("file")},
        {QStringLiteral("initiatedByUserGesture"), false},
        {QStringLiteral("initiatedFromOfficialUI"), false},
    };
    QDBusReply<QVariantList> listReply =
        iface.call(QStringLiteral("ListActionsWithContext"),
                   QStringLiteral("ContextMenu"),
                   listContext);
    QVERIFY(listReply.isValid());
    QVERIFY(listReply.value().isEmpty());

    const QVariantMap invokeContext{
        {QStringLiteral("capability"), QStringLiteral("ContextMenu")},
        {QStringLiteral("uris"), QStringList{QStringLiteral("file:///tmp/test.txt")}},
        {QStringLiteral("selection_count"), 1},
        {QStringLiteral("initiatedByUserGesture"), false},
        {QStringLiteral("initiatedFromOfficialUI"), false},
    };
    QDBusReply<QVariantMap> invokeReply =
        iface.call(QStringLiteral("InvokeActionWithContext"),
                   QStringLiteral("slm-test.desktop::Action"),
                   invokeContext);
    QVERIFY(invokeReply.isValid());
    const QVariantMap payload = invokeReply.value();
    QCOMPARE(payload.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(payload.value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));
}

void SlmCapabilityServiceDbusTest::denyAdminPermissionEndpointsForThirdPartyCaller()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    SlmCapabilityService service(nullptr);
    QVERIFY(service.serviceRegistered());

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    QDBusReply<QVariantList> grantsReply =
        iface.call(QStringLiteral("ListPermissionGrants"));
    QVERIFY(grantsReply.isValid());
    QVERIFY(grantsReply.value().isEmpty());

    QDBusReply<QVariantList> auditReply =
        iface.call(QStringLiteral("ListPermissionAudit"), 10);
    QVERIFY(auditReply.isValid());
    QVERIFY(auditReply.value().isEmpty());

    QDBusReply<QVariantMap> resetAppReply =
        iface.call(QStringLiteral("ResetPermissionsForApp"),
                   QStringLiteral("org.example.ThirdParty"));
    QVERIFY(resetAppReply.isValid());
    QCOMPARE(resetAppReply.value().value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(resetAppReply.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));

    QDBusReply<QVariantMap> resetAllReply =
        iface.call(QStringLiteral("ResetAllPermissions"));
    QVERIFY(resetAllReply.isValid());
    QCOMPARE(resetAllReply.value().value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(resetAllReply.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));
}

QTEST_MAIN(SlmCapabilityServiceDbusTest)
#include "slmcapabilityservice_dbus_test.moc"
