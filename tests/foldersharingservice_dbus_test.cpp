#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

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

    void listSharedFolders_usesFolderShareState()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QByteArray previousXdgDataHome = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", QFile::encodeName(tempDir.path()));

        const QString stateDir = QDir(tempDir.path()).filePath(QStringLiteral("slm-desktop"));
        QVERIFY(QDir().mkpath(stateDir));
        QFile stateFile(QDir(stateDir).filePath(QStringLiteral("folder_shares.json")));
        QVERIFY(stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QString sharedPath = QDir(tempDir.path()).filePath(QStringLiteral("Shared"));
        QVariantMap row{
            {QStringLiteral("enabled"), true},
            {QStringLiteral("shareName"), QStringLiteral("Shared")},
            {QStringLiteral("access"), QStringLiteral("all")},
            {QStringLiteral("permission"), QStringLiteral("write")},
            {QStringLiteral("allowGuest"), true},
        };
        QVariantMap stateMap;
        stateMap.insert(sharedPath, row);
        const QJsonObject state = QJsonObject::fromVariantMap(stateMap);
        stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
        stateFile.close();

        SharingManager manager;
        manager.initialize();
        const QVariantMap result = manager.listSharedFolders();
        QVERIFY(result.value(QStringLiteral("ok")).toBool());
        const QVariantMap folders = result.value(QStringLiteral("folders")).toMap();
        QVERIFY(folders.contains(sharedPath));
        const QVariantMap listed = folders.value(sharedPath).toMap();
        QCOMPARE(listed.value(QStringLiteral("path")).toString(), sharedPath);
        QCOMPARE(listed.value(QStringLiteral("permission")).toString(), QStringLiteral("write"));

        if (previousXdgDataHome.isEmpty())
            qunsetenv("XDG_DATA_HOME");
        else
            qputenv("XDG_DATA_HOME", previousXdgDataHome);
    }
};

QTEST_MAIN(SharingServiceCompatDbusTest)
#include "foldersharingservice_dbus_test.moc"
