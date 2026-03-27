#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/desktopd/foldersharingservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.FolderSharing";
constexpr const char kPath[] = "/org/slm/Desktop/FolderSharing";
constexpr const char kIface[] = "org.slm.Desktop.FolderSharing";
}

class FolderSharingServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void pingAndCapabilities_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        FolderSharingService service;
        if (!service.serviceRegistered()) {
            QSKIP("cannot register org.slm.Desktop.FolderSharing (likely already owned)");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
        QVERIFY(pingReply.isValid());
        const QVariantMap ping = pingReply.value();
        QVERIFY(ping.value(QStringLiteral("ok")).toBool());
        QCOMPARE(ping.value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));

        const QDBusReply<QVariantMap> capsReply = iface.call(QStringLiteral("GetCapabilities"));
        QVERIFY(capsReply.isValid());
        const QVariantMap caps = capsReply.value();
        QVERIFY(caps.value(QStringLiteral("ok")).toBool());
        const QStringList list = caps.value(QStringLiteral("capabilities")).toStringList();
        QVERIFY(list.contains(QStringLiteral("configure_share")));
        QVERIFY(list.contains(QStringLiteral("disable_share")));
        QVERIFY(list.contains(QStringLiteral("check_environment")));
        QVERIFY(list.contains(QStringLiteral("try_autofix")));
    }

    void checkEnvironment_contractShape()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        FolderSharingService service;
        if (!service.serviceRegistered()) {
            QSKIP("cannot register org.slm.Desktop.FolderSharing (likely already owned)");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QDBusReply<QVariantMap> envReply = iface.call(QStringLiteral("CheckEnvironment"));
        QVERIFY(envReply.isValid());
        const QVariantMap env = envReply.value();
        QVERIFY(env.contains(QStringLiteral("ok")));
        QVERIFY(env.contains(QStringLiteral("ready")));
        QVERIFY(env.contains(QStringLiteral("issues")));
        QVERIFY(env.value(QStringLiteral("issues")).canConvert<QVariantList>());
    }

    void tryAutoFix_contractShape()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        FolderSharingService service;
        if (!service.serviceRegistered()) {
            QSKIP("cannot register org.slm.Desktop.FolderSharing (likely already owned)");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QDBusReply<QVariantMap> fixReply = iface.call(QStringLiteral("TryAutoFix"));
        QVERIFY(fixReply.isValid());
        const QVariantMap fix = fixReply.value();
        QVERIFY(fix.contains(QStringLiteral("ok")));
        QVERIFY(fix.contains(QStringLiteral("ready")));
        QVERIFY(fix.contains(QStringLiteral("issues")));
        QVERIFY(fix.contains(QStringLiteral("actions")));
        QVERIFY(fix.value(QStringLiteral("issues")).canConvert<QVariantList>());
        QVERIFY(fix.value(QStringLiteral("actions")).canConvert<QVariantList>());
    }

    void configureShare_invalidPathRejected()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }

        FolderSharingService service;
        if (!service.serviceRegistered()) {
            QSKIP("cannot register org.slm.Desktop.FolderSharing (likely already owned)");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QVariantMap options{
            {QStringLiteral("enabled"), true},
            {QStringLiteral("shareName"), QStringLiteral("InvalidPathCase")}
        };
        const QDBusReply<QVariantMap> reply =
                iface.call(QStringLiteral("ConfigureShare"),
                           QStringLiteral("/definitely/not/an/existing/folder"),
                           options);
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("not-a-directory"));
    }
};

QTEST_MAIN(FolderSharingServiceDbusTest)
#include "foldersharingservice_dbus_test.moc"
