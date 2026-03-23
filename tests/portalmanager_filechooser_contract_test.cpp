#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "../portalmethodnames.h"
#include "../src/daemon/portald/portalmanager.h"

namespace {
constexpr const char kUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kUiPath[] = "/org/slm/Desktop/PortalUI";
}

class FakePortalUiService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.PortalUI")

public:
    QVariantMap lastOptions;
    QVariantMap response{
        {QStringLiteral("ok"), true},
        {QStringLiteral("path"), QStringLiteral("/tmp/mock")},
    };

public slots:
    QVariantMap FileChooser(const QVariantMap &options)
    {
        lastOptions = options;
        return response;
    }
};

class PortalManagerFileChooserContractTest : public QObject
{
    Q_OBJECT

private:
    QDBusConnection m_bus = QDBusConnection::sessionBus();

    bool registerFake(FakePortalUiService &fake)
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

    void unregisterFake()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
    }

private slots:
    void cleanup()
    {
        unregisterFake();
    }

    void fileChooser_forwardsOptionsAndResponse()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        FakePortalUiService fake;
        if (!registerFake(fake)) {
            QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
        }

        fake.response = {
            {QStringLiteral("ok"), true},
            {QStringLiteral("path"), QStringLiteral("/home/garis/test.txt")},
            {QStringLiteral("mode"), QStringLiteral("open")},
        };

        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("open")},
            {QStringLiteral("title"), QStringLiteral("Open")},
            {QStringLiteral("multiple"), false},
        };
        const QVariantMap out = manager.FileChooser(options);

        QVERIFY(out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(out.value(QStringLiteral("path")).toString(), QStringLiteral("/home/garis/test.txt"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("mode")).toString(), QStringLiteral("open"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("title")).toString(), QStringLiteral("Open"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("multiple")).toBool(), false);
    }

    void pickFolder_forcesFolderSemantics()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        FakePortalUiService fake;
        if (!registerFake(fake)) {
            QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
        }

        fake.response = {
            {QStringLiteral("ok"), true},
            {QStringLiteral("path"), QStringLiteral("/home/garis/Pictures")},
            {QStringLiteral("mode"), QStringLiteral("folder")},
        };

        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("folder")},
            {QStringLiteral("title"), QStringLiteral("Choose Folder")},
            {QStringLiteral("multiple"), false},
            {QStringLiteral("selectFolders"), true},
        };
        const QVariantMap out = manager.PickFolder(options);

        QVERIFY(out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kPickFolder));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("mode")).toString(), QStringLiteral("folder"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("selectFolders")).toBool(), true);
        QCOMPARE(fake.lastOptions.value(QStringLiteral("multiple")).toBool(), false);
        QCOMPARE(fake.lastOptions.value(QStringLiteral("title")).toString(), QStringLiteral("Choose Folder"));
    }

    void fileChooser_invalidMode_returnsInvalidArgument()
    {
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("weird-mode")},
            {QStringLiteral("title"), QStringLiteral("Open")},
        };
        const QVariantMap out = manager.FileChooser(options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
    }

    void fileChooser_cancelPayload_passthroughContract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        FakePortalUiService fake;
        if (!registerFake(fake)) {
            QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
        }

        fake.response = {
            {QStringLiteral("ok"), false},
            {QStringLiteral("canceled"), true},
            {QStringLiteral("error"), QStringLiteral("canceled")},
            {QStringLiteral("path"), QString()},
            {QStringLiteral("paths"), QVariantList{}},
        };

        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("open")},
            {QStringLiteral("title"), QStringLiteral("Open")},
            {QStringLiteral("multiple"), false},
        };
        const QVariantMap out = manager.FileChooser(options);

        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("canceled")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("canceled"));
        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(out.value(QStringLiteral("path")).toString(), QString());
        QCOMPARE(out.value(QStringLiteral("paths")).toList().size(), 0);
        QCOMPARE(fake.lastOptions.value(QStringLiteral("mode")).toString(), QStringLiteral("open"));
    }

    void pickFolder_cancelPayload_passthroughContract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        FakePortalUiService fake;
        if (!registerFake(fake)) {
            QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
        }

        fake.response = {
            {QStringLiteral("ok"), false},
            {QStringLiteral("canceled"), true},
            {QStringLiteral("error"), QStringLiteral("canceled")},
            {QStringLiteral("path"), QString()},
            {QStringLiteral("paths"), QVariantList{}},
        };

        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("folder")},
            {QStringLiteral("title"), QStringLiteral("Choose Folder")},
            {QStringLiteral("multiple"), false},
            {QStringLiteral("selectFolders"), true},
        };
        const QVariantMap out = manager.PickFolder(options);

        QVERIFY(!out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("canceled")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("canceled"));
        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kPickFolder));
        QCOMPARE(out.value(QStringLiteral("path")).toString(), QString());
        QCOMPARE(out.value(QStringLiteral("paths")).toList().size(), 0);

        // PickFolder forces folder semantics to UI bridge.
        QCOMPARE(fake.lastOptions.value(QStringLiteral("mode")).toString(), QStringLiteral("folder"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("selectFolders")).toBool(), true);
        QCOMPARE(fake.lastOptions.value(QStringLiteral("multiple")).toBool(), false);
    }

    void fileChooser_multiSelectPayload_passthroughContract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        FakePortalUiService fake;
        if (!registerFake(fake)) {
            QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
        }

        const QVariantList paths{
            QStringLiteral("/home/garis/Pictures/a.png"),
            QStringLiteral("/home/garis/Pictures/b.png"),
        };
        const QVariantList uris{
            QStringLiteral("file:///home/garis/Pictures/a.png"),
            QStringLiteral("file:///home/garis/Pictures/b.png"),
        };
        fake.response = {
            {QStringLiteral("ok"), true},
            {QStringLiteral("mode"), QStringLiteral("open")},
            {QStringLiteral("path"), paths.first().toString()},
            {QStringLiteral("uri"), uris.first().toString()},
            {QStringLiteral("paths"), paths},
            {QStringLiteral("uris"), uris},
        };

        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("open")},
            {QStringLiteral("title"), QStringLiteral("Open")},
            {QStringLiteral("multiple"), true},
        };
        const QVariantMap out = manager.FileChooser(options);

        QVERIFY(out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(out.value(QStringLiteral("mode")).toString(), QStringLiteral("open"));
        QCOMPARE(out.value(QStringLiteral("path")).toString(), paths.first().toString());
        QCOMPARE(out.value(QStringLiteral("uri")).toString(), uris.first().toString());
        QCOMPARE(out.value(QStringLiteral("paths")).toList(), paths);
        QCOMPARE(out.value(QStringLiteral("uris")).toList(), uris);
        QCOMPARE(fake.lastOptions.value(QStringLiteral("multiple")).toBool(), true);
    }

    void fileChooser_savePayload_passthroughContract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        FakePortalUiService fake;
        if (!registerFake(fake)) {
            QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
        }

        fake.response = {
            {QStringLiteral("ok"), true},
            {QStringLiteral("mode"), QStringLiteral("save")},
            {QStringLiteral("path"), QStringLiteral("/home/garis/Pictures/Screenshots/shot.png")},
            {QStringLiteral("uri"), QStringLiteral("file:///home/garis/Pictures/Screenshots/shot.png")},
            {QStringLiteral("canceled"), false},
        };

        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("save")},
            {QStringLiteral("title"), QStringLiteral("Save Image As")},
            {QStringLiteral("folder"), QStringLiteral("/home/garis/Pictures/Screenshots")},
            {QStringLiteral("name"), QStringLiteral("shot.png")},
            {QStringLiteral("multiple"), false},
        };
        const QVariantMap out = manager.FileChooser(options);

        QVERIFY(out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(out.value(QStringLiteral("mode")).toString(), QStringLiteral("save"));
        QCOMPARE(out.value(QStringLiteral("path")).toString(),
                 QStringLiteral("/home/garis/Pictures/Screenshots/shot.png"));
        QCOMPARE(out.value(QStringLiteral("uri")).toString(),
                 QStringLiteral("file:///home/garis/Pictures/Screenshots/shot.png"));
        QCOMPARE(out.value(QStringLiteral("canceled")).toBool(), false);

        QCOMPARE(fake.lastOptions.value(QStringLiteral("mode")).toString(), QStringLiteral("save"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("folder")).toString(),
                 QStringLiteral("/home/garis/Pictures/Screenshots"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("name")).toString(), QStringLiteral("shot.png"));
        QCOMPARE(fake.lastOptions.value(QStringLiteral("multiple")).toBool(), false);
    }

    void pickFolder_conflictingOptions_returnsInvalidArgument()
    {
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("open")},
            {QStringLiteral("multiple"), true},
            {QStringLiteral("selectFolders"), false},
        };
        const QVariantMap out = manager.PickFolder(options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kPickFolder));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
    }
};

QTEST_MAIN(PortalManagerFileChooserContractTest)
#include "portalmanager_filechooser_contract_test.moc"
