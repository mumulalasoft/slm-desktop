#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/daemon/portald/implportalservice.h"
#include "../src/daemon/portald/portalmanager.h"

namespace {
constexpr const char kService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kIfaceFileChooser[] = "org.freedesktop.impl.portal.FileChooser";
constexpr const char kIfaceOpenUri[] = "org.freedesktop.impl.portal.OpenURI";
constexpr const char kIfaceScreenshot[] = "org.freedesktop.impl.portal.Screenshot";
constexpr const char kIfaceScreenCast[] = "org.freedesktop.impl.portal.ScreenCast";
constexpr const char kIfaceSettings[] = "org.freedesktop.impl.portal.Settings";
constexpr const char kIfaceNotification[] = "org.freedesktop.impl.portal.Notification";
constexpr const char kIfaceInhibit[] = "org.freedesktop.impl.portal.Inhibit";
constexpr const char kIfaceOpenWith[] = "org.freedesktop.impl.portal.OpenWith";
constexpr const char kIfaceDocuments[] = "org.freedesktop.impl.portal.Documents";
constexpr const char kIfaceTrash[] = "org.freedesktop.impl.portal.Trash";
constexpr const char kUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kUiPath[] = "/org/slm/Desktop/PortalUI";
}

class FakePortalUiServiceSnapshot : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.PortalUI")

public:
    QVariantMap response{
        {QStringLiteral("ok"), true},
        {QStringLiteral("path"), QStringLiteral("/home/garis/Pictures/a.png")},
    };

public slots:
    QVariantMap FileChooser(const QVariantMap &)
    {
        return response;
    }
};

class ImplPortalPayloadSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void snapshot_requiredFieldsAcrossMethods();

private:
    QDBusConnection m_bus = QDBusConnection::sessionBus();

    bool registerFakeUi(FakePortalUiServiceSnapshot &fake)
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

    void unregisterFakeUi()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kUiPath));
        m_bus.unregisterService(QString::fromLatin1(kUiService));
    }
};

void ImplPortalPayloadSnapshotTest::snapshot_requiredFieldsAcrossMethods()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    FakePortalUiServiceSnapshot fake;
    if (!registerFakeUi(fake)) {
        QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
    }

    PortalManager manager;
    ImplPortalService service(&manager, nullptr);
    if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

    auto assertCommon = [](const QVariantMap &out) {
        QVERIFY(out.contains(QStringLiteral("ok")));
        QVERIFY(out.contains(QStringLiteral("response")));
        QVERIFY(out.contains(QStringLiteral("results")));
        QVERIFY(out.contains(QStringLiteral("request_handle")));
    };

    // OpenURI
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceOpenUri),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("OpenURI"),
                      QStringLiteral("/request/snap-openuri"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      QStringLiteral("https://example.com"),
                      QVariantMap{{QStringLiteral("dryRun"), true}});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        assertCommon(out);
        const QVariantMap results = out.value(QStringLiteral("results")).toMap();
        QVERIFY(results.contains(QStringLiteral("uri")));
        QVERIFY(results.contains(QStringLiteral("handled")));
    }

    // FileChooser
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceFileChooser),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("OpenFile"),
                      QStringLiteral("/request/snap-chooser"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      QVariantMap{{QStringLiteral("mode"), QStringLiteral("open")}});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        assertCommon(out);
        const QVariantMap results = out.value(QStringLiteral("results")).toMap();
        QVERIFY(results.contains(QStringLiteral("uris")));
        QVERIFY(results.contains(QStringLiteral("choices")));
        QVERIFY(results.contains(QStringLiteral("current_filter")));
    }

    // Screenshot
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceScreenshot),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("Screenshot"),
                      QStringLiteral("/request/snap-shot"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        assertCommon(out);
        const QVariantMap results = out.value(QStringLiteral("results")).toMap();
        QVERIFY(results.contains(QStringLiteral("uri")));
    }

    // ScreenCast
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceScreenCast),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> createReply =
            iface.call(QStringLiteral("CreateSession"),
                      QStringLiteral("/request/snap-cast-create"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      QVariantMap{{QStringLiteral("initiatedByUserGesture"), true}});
        QVERIFY(createReply.isValid());
        const QVariantMap createOut = createReply.value();
        assertCommon(createOut);
        QVERIFY(createOut.contains(QStringLiteral("session_handle")));
        const QVariantMap createResults = createOut.value(QStringLiteral("results")).toMap();
        QVERIFY(createResults.contains(QStringLiteral("session_handle")));
        QVERIFY(createResults.contains(QStringLiteral("streams")));

        const QString session = QStringLiteral("/org/desktop/portal/session/org_example_App/snap");
        QDBusReply<QVariantMap> selectReply =
            iface.call(QStringLiteral("SelectSources"),
                      QStringLiteral("/request/snap-cast-select"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      session,
                      QVariantMap{});
        QVERIFY(selectReply.isValid());
        const QVariantMap selectResults = selectReply.value().value(QStringLiteral("results")).toMap();
        QVERIFY(selectResults.contains(QStringLiteral("session_handle")));
        QVERIFY(selectResults.contains(QStringLiteral("sources_selected")));
        QVERIFY(selectResults.contains(QStringLiteral("selected_sources")));
        if (selectReply.value().value(QStringLiteral("ok")).toBool()) {
            QVERIFY(selectResults.value(QStringLiteral("sources_selected")).toBool());
        }

        QDBusReply<QVariantMap> startReply =
            iface.call(QStringLiteral("Start"),
                      QStringLiteral("/request/snap-cast-start"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      session,
                      QVariantMap{});
        QVERIFY(startReply.isValid());
        const QVariantMap startResults = startReply.value().value(QStringLiteral("results")).toMap();
        QVERIFY(startResults.contains(QStringLiteral("session_handle")));
        QVERIFY(startResults.contains(QStringLiteral("streams")));
        const QVariantList streams = startResults.value(QStringLiteral("streams")).toList();
        if (startReply.value().value(QStringLiteral("ok")).toBool()) {
            QVERIFY(!streams.isEmpty());
            const QVariantMap firstStream = streams.constFirst().toMap();
            QVERIFY(firstStream.contains(QStringLiteral("node_id")));
            QVERIFY(firstStream.contains(QStringLiteral("stream_id")));
            QVERIFY(firstStream.contains(QStringLiteral("source_type")));
        }

        QDBusReply<QVariantMap> stopReply =
            iface.call(QStringLiteral("Stop"),
                      QStringLiteral("/request/snap-cast-stop"),
                      QStringLiteral("org.example.App"),
                      QStringLiteral("x11:1"),
                      session,
                      QVariantMap{});
        QVERIFY(stopReply.isValid());
        const QVariantMap stopResults = stopReply.value().value(QStringLiteral("results")).toMap();
        QVERIFY(stopResults.contains(QStringLiteral("session_handle")));
        QVERIFY(stopResults.contains(QStringLiteral("stopped")));
        QVERIFY(stopResults.contains(QStringLiteral("session_closed")));
    }

    // Settings
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceSettings),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> allReply =
            iface.call(QStringLiteral("ReadAll"),
                      QStringList{
                          QStringLiteral("org.freedesktop.appearance"),
                          QStringLiteral("org.freedesktop.desktop.interface"),
                      });
        QVERIFY(allReply.isValid());
        const QVariantMap all = allReply.value();
        QVERIFY(all.contains(QStringLiteral("org.freedesktop.appearance")));
        QVERIFY(all.contains(QStringLiteral("org.freedesktop.desktop.interface")));
    }

    // Notification
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceNotification),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("AddNotification"),
                      QStringLiteral("org.example.App.desktop"),
                      QStringLiteral("snap-notif"),
                      QVariantMap{{QStringLiteral("title"), QStringLiteral("hello")}});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(out.contains(QStringLiteral("ok")));
        QVERIFY(out.contains(QStringLiteral("response")));
        QVERIFY(out.contains(QStringLiteral("results")));
    }

    // Inhibit
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceInhibit),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("Inhibit"),
                      QStringLiteral("/request/snap-inhibit"),
                      QStringLiteral("org.example.App.desktop"),
                      QStringLiteral("x11:1"),
                      uint(0),
                      QVariantMap{{QStringLiteral("reason"), QStringLiteral("snapshot")}});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(out.contains(QStringLiteral("ok")));
        QVERIFY(out.contains(QStringLiteral("response")));
        QVERIFY(out.contains(QStringLiteral("results")));
    }

    // OpenWith
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceOpenWith),
                             m_bus);
        QVERIFY(iface.isValid());
        const QString samplePath = QStringLiteral("/etc/hosts");
        QDBusReply<QVariantMap> handlersReply =
            iface.call(QStringLiteral("QueryHandlers"),
                      QStringLiteral("/request/snap-openwith-query"),
                      QStringLiteral("org.example.App.desktop"),
                      QStringLiteral("x11:1"),
                      samplePath,
                      QVariantMap{{QStringLiteral("limit"), 6}});
        QVERIFY(handlersReply.isValid());
        const QVariantMap handlersOut = handlersReply.value();
        QVERIFY(handlersOut.contains(QStringLiteral("ok")));
        QVERIFY(handlersOut.contains(QStringLiteral("response")));
        const QVariantMap handlersResults = handlersOut.value(QStringLiteral("results")).toMap();
        QVERIFY(handlersResults.contains(QStringLiteral("handlers")));

        QDBusReply<QVariantMap> openReply =
            iface.call(QStringLiteral("OpenFileWith"),
                      QStringLiteral("/request/snap-openwith-open"),
                      QStringLiteral("org.example.App.desktop"),
                      QStringLiteral("x11:1"),
                      samplePath,
                      QStringLiteral("org.example.Dummy.desktop"),
                      QVariantMap{{QStringLiteral("dryRun"), true}});
        QVERIFY(openReply.isValid());
        const QVariantMap openOut = openReply.value();
        QVERIFY(openOut.contains(QStringLiteral("ok")));
        QVERIFY(openOut.contains(QStringLiteral("response")));
        QVERIFY(openOut.value(QStringLiteral("results")).toMap().contains(QStringLiteral("handler")));
    }

    // Documents
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceDocuments),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> addReply =
            iface.call(QStringLiteral("Add"),
                      QStringLiteral("/request/snap-doc-add"),
                      QStringLiteral("org.example.App.desktop"),
                      QStringLiteral("x11:1"),
                      QStringList{QStringLiteral("file:///etc/hosts")},
                      QVariantMap{});
        QVERIFY(addReply.isValid());
        const QVariantMap addOut = addReply.value();
        QVERIFY(addOut.contains(QStringLiteral("ok")));
        QVERIFY(addOut.contains(QStringLiteral("response")));
        QVERIFY(addOut.contains(QStringLiteral("results")));
    }

    // Trash
    {
        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIfaceTrash),
                             m_bus);
        QVERIFY(iface.isValid());
        QDBusReply<QVariantMap> reply =
            iface.call(QStringLiteral("TrashFile"),
                      QStringLiteral("/request/snap-trash"),
                      QStringLiteral("org.example.App.desktop"),
                      QStringLiteral("x11:1"),
                      QStringLiteral(""),
                      QVariantMap{});
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(out.contains(QStringLiteral("ok")));
        QVERIFY(out.contains(QStringLiteral("response")));
        QVERIFY(out.contains(QStringLiteral("results")));
    }

    unregisterFakeUi();
}

QTEST_MAIN(ImplPortalPayloadSnapshotTest)
#include "implportal_payload_snapshot_test.moc"
