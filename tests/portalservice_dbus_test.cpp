#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#include "../portalmethodnames.h"
#include "../src/daemon/portald/portalmanager.h"
#include "../src/daemon/portald/portalservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Portal";
constexpr const char kPath[] = "/org/slm/Desktop/Portal";
constexpr const char kIface[] = "org.slm.Desktop.Portal";
constexpr const char kUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kUiPath[] = "/org/slm/Desktop/PortalUI";
constexpr const char kSearchService[] = "org.slm.Desktop.Search.v1";
constexpr const char kSearchPath[] = "/org/slm/Desktop/Search";
constexpr const char kCapabilityService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kCapabilityPath[] = "/org/freedesktop/SLMCapabilities";
}

class FakePortalUiService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.PortalUI")

public:
    QVariantMap lastOptions;
    QVariantMap response{
        {QStringLiteral("ok"), true},
    };

public slots:
    QVariantMap FileChooser(const QVariantMap &options)
    {
        lastOptions = options;
        return response;
    }
};

class FakeSearchService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Search.v1")

public:
    QVariantList rows;
    QVariantMap lastOptions;
    QString lastQuery;

public slots:
    QVariantList Query(const QString &query, const QVariantMap &options)
    {
        lastQuery = query;
        lastOptions = options;
        return rows;
    }
};

class FakePortalBackend : public QObject
{
    Q_OBJECT

public slots:
    QVariantMap handleRequest(const QString &portalMethod,
                              const QDBusMessage &,
                              const QVariantMap &) const
    {
        if (portalMethod == QStringLiteral("ReadNotificationHistoryItem")) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
                {QStringLiteral("method"), portalMethod},
                {QStringLiteral("reason"), QStringLiteral("default-deny")},
            };
        }
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("method"), portalMethod},
        };
    }

    QVariantMap handleDirect(const QString &portalMethod,
                             const QDBusMessage &,
                             const QVariantMap &) const
    {
        if (portalMethod == QStringLiteral("QueryFiles")
            || portalMethod == QStringLiteral("QueryClipboardPreview")) {
            return {
                {QStringLiteral("ok"), true},
                {QStringLiteral("authorized"), true},
            };
        }
        if (portalMethod == QStringLiteral("QueryNotificationHistoryPreview")) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
                {QStringLiteral("method"), portalMethod},
                {QStringLiteral("reason"), QStringLiteral("default-deny")},
            };
        }
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("mediation-required-use-request-flow")},
            {QStringLiteral("method"), portalMethod},
        };
    }
};

class FakeCapabilityService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.SLMCapabilities")

public:
    QVariantList actionRows;
    QVariantList contextRows;

public slots:
    QVariantList SearchActions(const QString &, const QVariantMap &)
    {
        return actionRows;
    }

    QVariantList ListActionsWithContext(const QString &, const QVariantMap &)
    {
        return contextRows;
    }
};

class PortalServiceDbusTest : public QObject
{
    Q_OBJECT

private:
    QDBusConnection m_bus = QDBusConnection::sessionBus();

    bool registerFakeUi(FakePortalUiService &fake)
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

    bool registerFakeSearch(FakeSearchService &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSearchService)).value()) {
            return false;
        }
        if (!m_bus.registerService(QString::fromLatin1(kSearchService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kSearchPath),
                                  &fake,
                                  QDBusConnection::ExportAllSlots)) {
            m_bus.unregisterService(QString::fromLatin1(kSearchService));
            return false;
        }
        return true;
    }

    void unregisterFakeSearch()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kSearchPath));
        m_bus.unregisterService(QString::fromLatin1(kSearchService));
    }

    bool registerFakeCapabilities(FakeCapabilityService &fake)
    {
        if (!m_bus.isConnected()) {
            return false;
        }
        QDBusConnectionInterface *iface = m_bus.interface();
        if (!iface) {
            return false;
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kCapabilityService)).value()) {
            return false;
        }
        if (!m_bus.registerService(QString::fromLatin1(kCapabilityService))) {
            return false;
        }
        if (!m_bus.registerObject(QString::fromLatin1(kCapabilityPath),
                                  &fake,
                                  QDBusConnection::ExportAllSlots)) {
            m_bus.unregisterService(QString::fromLatin1(kCapabilityService));
            return false;
        }
        return true;
    }

    void unregisterFakeCapabilities()
    {
        if (!m_bus.isConnected()) {
            return;
        }
        m_bus.unregisterObject(QString::fromLatin1(kCapabilityPath));
        m_bus.unregisterService(QString::fromLatin1(kCapabilityService));
    }

private slots:
    void cleanup()
    {
        unregisterFakeUi();
        unregisterFakeSearch();
        unregisterFakeCapabilities();
    }

    void pingAndCapabilities_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        PortalManager manager;
        PortalService service(&manager);
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
        const QVariantMap ping = pingReply.value();
        QVERIFY(ping.value(QStringLiteral("ok")).toBool());
        QCOMPARE(ping.value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));
        QCOMPARE(ping.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));

        QDBusReply<QVariantMap> capsReply = iface.call(QStringLiteral("GetCapabilities"));
        QVERIFY(capsReply.isValid());
        const QVariantMap caps = capsReply.value();
        QVERIFY(caps.value(QStringLiteral("ok")).toBool());
        QCOMPARE(caps.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));
        const QStringList list = caps.value(QStringLiteral("capabilities")).toStringList();
        QVERIFY(list.contains(QStringLiteral("screenshot")));
        QVERIFY(list.contains(QStringLiteral("wallpaper")));
    }

    void methods_existAndReturnStructuredPlaceholder()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        PortalManager manager;
        PortalService service(&manager);
        if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QVariantMap options{
            {QStringLiteral("test"), true},
            {QStringLiteral("mode"), QStringLiteral("open")},
        };

        auto verifyNotImplemented = [](const QVariantMap &out, const QString &method) {
            QVERIFY(!out.value(QStringLiteral("ok")).toBool());
            QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("not-implemented"));
            QCOMPARE(out.value(QStringLiteral("method")).toString(), method);
        };

        QDBusReply<QVariantMap> screenshotReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kScreenshot), options);
        QVERIFY(screenshotReply.isValid());
        verifyNotImplemented(screenshotReply.value(), QString::fromLatin1(SlmPortalMethod::kScreenshot));
        QCOMPARE(screenshotReply.value().value(QStringLiteral("options")).toMap(), options);

        QDBusReply<QVariantMap> screencastReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kScreenCast), options);
        QVERIFY(screencastReply.isValid());
        verifyNotImplemented(screencastReply.value(), QString::fromLatin1(SlmPortalMethod::kScreenCast));
        QCOMPARE(screencastReply.value().value(QStringLiteral("options")).toMap(), options);

        QDBusReply<QVariantMap> chooserReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kFileChooser), options);
        QVERIFY(chooserReply.isValid());
        {
            const QVariantMap out = chooserReply.value();
            QVERIFY(!out.value(QStringLiteral("ok")).toBool());
            const QString err = out.value(QStringLiteral("error")).toString();
            QVERIFY(err == QStringLiteral("not-implemented")
                    || err == QStringLiteral("ui-service-unavailable")
                    || err == QStringLiteral("ui-call-failed"));
            QCOMPARE(out.value(QStringLiteral("method")).toString(),
                     QString::fromLatin1(SlmPortalMethod::kFileChooser));
            QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
        }

        QVariantMap openUriOptions = options;
        openUriOptions.insert(QStringLiteral("dryRun"), true);
        QDBusReply<QVariantMap> openUriReply = iface.call(QString::fromLatin1(SlmPortalMethod::kOpenURI),
                                                          QStringLiteral("https://example.com"),
                                                          openUriOptions);
        QVERIFY(openUriReply.isValid());
        QVERIFY(openUriReply.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(openUriReply.value().value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(openUriReply.value().value(QStringLiteral("uri")).toString(),
                 QStringLiteral("https://example.com"));
        QCOMPARE(openUriReply.value().value(QStringLiteral("options")).toMap(), openUriOptions);
        QCOMPARE(openUriReply.value().value(QStringLiteral("dryRun")).toBool(), true);
        QCOMPARE(openUriReply.value().value(QStringLiteral("launched")).toBool(), false);

        QDBusReply<QVariantMap> colorReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kPickColor), options);
        QVERIFY(colorReply.isValid());
        verifyNotImplemented(colorReply.value(), QString::fromLatin1(SlmPortalMethod::kPickColor));
        QCOMPARE(colorReply.value().value(QStringLiteral("options")).toMap(), options);

        const QVariantMap pickFolderOptions{
            {QStringLiteral("test"), true},
            {QStringLiteral("mode"), QStringLiteral("folder")},
            {QStringLiteral("selectFolders"), true},
            {QStringLiteral("multiple"), false},
        };
        QDBusReply<QVariantMap> folderReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kPickFolder), pickFolderOptions);
        QVERIFY(folderReply.isValid());
        {
            const QVariantMap out = folderReply.value();
            QVERIFY(!out.value(QStringLiteral("ok")).toBool());
            const QString err = out.value(QStringLiteral("error")).toString();
            QVERIFY(err == QStringLiteral("not-implemented")
                    || err == QStringLiteral("ui-service-unavailable")
                    || err == QStringLiteral("ui-call-failed"));
            QCOMPARE(out.value(QStringLiteral("method")).toString(),
                     QString::fromLatin1(SlmPortalMethod::kPickFolder));
        }

        QDBusReply<QVariantMap> wallpaperReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kWallpaper), options);
        QVERIFY(wallpaperReply.isValid());
        verifyNotImplemented(wallpaperReply.value(), QString::fromLatin1(SlmPortalMethod::kWallpaper));
        QCOMPARE(wallpaperReply.value().value(QStringLiteral("options")).toMap(), options);
    }

    void malformedPayload_stillReturnsStructuredError()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        PortalManager manager;
        PortalService service(&manager);
        if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        const QVariantMap emptyOptions;

        QDBusReply<QVariantMap> openUriEmptyReply = iface.call(
                                                               QString::fromLatin1(SlmPortalMethod::kOpenURI),
                                                               QString(),
                                                               emptyOptions);
        QVERIFY(openUriEmptyReply.isValid());
        const QVariantMap openUriOut = openUriEmptyReply.value();
        QVERIFY(!openUriOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(openUriOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(openUriOut.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(openUriOut.value(QStringLiteral("uri")).toString(), QString());
        QCOMPARE(openUriOut.value(QStringLiteral("options")).toMap(), emptyOptions);

        QDBusReply<QVariantMap> screenshotEmptyReply = iface.call(
                                                                  QString::fromLatin1(SlmPortalMethod::kScreenshot),
                                                                  emptyOptions);
        QVERIFY(screenshotEmptyReply.isValid());
        const QVariantMap screenshotOut = screenshotEmptyReply.value();
        QVERIFY(!screenshotOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(screenshotOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kScreenshot));
        QCOMPARE(screenshotOut.value(QStringLiteral("error")).toString(), QStringLiteral("not-implemented"));
        QCOMPARE(screenshotOut.value(QStringLiteral("options")).toMap(), emptyOptions);

        QDBusReply<QVariantMap> wallpaperEmptyReply = iface.call(
                                                                 QString::fromLatin1(SlmPortalMethod::kWallpaper),
                                                                 emptyOptions);
        QVERIFY(wallpaperEmptyReply.isValid());
        const QVariantMap wallpaperOut = wallpaperEmptyReply.value();
        QVERIFY(!wallpaperOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(wallpaperOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kWallpaper));
        QCOMPARE(wallpaperOut.value(QStringLiteral("error")).toString(), QStringLiteral("not-implemented"));
        QCOMPARE(wallpaperOut.value(QStringLiteral("options")).toMap(), emptyOptions);

        QDBusReply<QVariantMap> chooserInvalidReply = iface.call(
                                                                 QString::fromLatin1(SlmPortalMethod::kFileChooser),
                                                                 emptyOptions);
        QVERIFY(chooserInvalidReply.isValid());
        const QVariantMap chooserInvalidOut = chooserInvalidReply.value();
        QVERIFY(!chooserInvalidOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(chooserInvalidOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(chooserInvalidOut.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(chooserInvalidOut.value(QStringLiteral("options")).toMap(), emptyOptions);

        const QVariantMap pickFolderInvalidOptions{
            {QStringLiteral("mode"), QStringLiteral("open")},
            {QStringLiteral("selectFolders"), false},
            {QStringLiteral("multiple"), true},
        };
        QDBusReply<QVariantMap> pickFolderInvalidReply = iface.call(
                                                                    QString::fromLatin1(SlmPortalMethod::kPickFolder),
                                                                    pickFolderInvalidOptions);
        QVERIFY(pickFolderInvalidReply.isValid());
        const QVariantMap pickFolderInvalidOut = pickFolderInvalidReply.value();
        QVERIFY(!pickFolderInvalidOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(pickFolderInvalidOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kPickFolder));
        QCOMPARE(pickFolderInvalidOut.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(pickFolderInvalidOut.value(QStringLiteral("options")).toMap(), pickFolderInvalidOptions);
    }

    void chooser_passthroughPayload_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        FakePortalUiService fake;
        if (!registerFakeUi(fake)) {
            QSKIP("cannot register fake org.slm.Desktop.PortalUI service/object");
        }

        PortalManager manager;
        PortalService service(&manager);
        if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        fake.response = {
            {QStringLiteral("ok"), false},
            {QStringLiteral("canceled"), true},
            {QStringLiteral("error"), QStringLiteral("canceled")},
            {QStringLiteral("path"), QString()},
            {QStringLiteral("paths"), QVariantList{}},
        };
        QDBusReply<QVariantMap> canceledReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kFileChooser),
            QVariantMap{
                {QStringLiteral("mode"), QStringLiteral("open")},
                {QStringLiteral("multiple"), false},
            });
        QVERIFY(canceledReply.isValid());
        const QVariantMap canceledOut = canceledReply.value();
        QVERIFY(!canceledOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(canceledOut.value(QStringLiteral("canceled")).toBool(), true);
        QCOMPARE(canceledOut.value(QStringLiteral("error")).toString(), QStringLiteral("canceled"));
        QCOMPARE(canceledOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));

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
        QDBusReply<QVariantMap> multiReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kFileChooser),
            QVariantMap{
                {QStringLiteral("mode"), QStringLiteral("open")},
                {QStringLiteral("multiple"), true},
            });
        QVERIFY(multiReply.isValid());
        const QVariantMap multiOut = multiReply.value();
        QVERIFY(multiOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(multiOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(multiOut.value(QStringLiteral("paths")).toList(), paths);
        QCOMPARE(multiOut.value(QStringLiteral("uris")).toList(), uris);

        fake.response = {
            {QStringLiteral("ok"), true},
            {QStringLiteral("mode"), QStringLiteral("save")},
            {QStringLiteral("path"), QStringLiteral("/home/garis/Pictures/Screenshots/shot.png")},
            {QStringLiteral("uri"), QStringLiteral("file:///home/garis/Pictures/Screenshots/shot.png")},
            {QStringLiteral("canceled"), false},
        };
        QDBusReply<QVariantMap> saveReply = iface.call(
            QString::fromLatin1(SlmPortalMethod::kFileChooser),
            QVariantMap{
                {QStringLiteral("mode"), QStringLiteral("save")},
                {QStringLiteral("folder"), QStringLiteral("/home/garis/Pictures/Screenshots")},
                {QStringLiteral("name"), QStringLiteral("shot.png")},
                {QStringLiteral("multiple"), false},
            });
        QVERIFY(saveReply.isValid());
        const QVariantMap saveOut = saveReply.value();
        QVERIFY(saveOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(saveOut.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
        QCOMPARE(saveOut.value(QStringLiteral("mode")).toString(), QStringLiteral("save"));
        QCOMPARE(saveOut.value(QStringLiteral("path")).toString(),
                 QStringLiteral("/home/garis/Pictures/Screenshots/shot.png"));
        QCOMPARE(saveOut.value(QStringLiteral("canceled")).toBool(), false);
    }

    void clipboardRequests_validationContract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        PortalManager manager;
        PortalService service(&manager);
        if (!service.serviceRegistered()) {
        QSKIP("cannot register service name on session bus (likely already owned)");
    }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        // Full-content request must include identity for target item.
        QDBusReply<QVariantMap> missingIdReply =
            iface.call(QStringLiteral("RequestClipboardContent"), QVariantMap{});
        QVERIFY(missingIdReply.isValid());
        const QVariantMap missingIdOut = missingIdReply.value();
        QVERIFY(!missingIdOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(missingIdOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("RequestClipboardContent"));
        QCOMPARE(missingIdOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("invalid-argument"));

        // Full-content request is gesture-gated even before backend routing.
        QDBusReply<QVariantMap> noGestureReply =
            iface.call(QStringLiteral("RequestClipboardContent"),
                       QVariantMap{
                           {QStringLiteral("clipboardId"), 42},
                           {QStringLiteral("initiatedByUserGesture"), false},
                       });
        QVERIFY(noGestureReply.isValid());
        const QVariantMap noGestureOut = noGestureReply.value();
        QVERIFY(!noGestureOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(noGestureOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("RequestClipboardContent"));
        QCOMPARE(noGestureOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));
        QCOMPARE(noGestureOut.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("user-gesture-required"));

        // High-risk resolve endpoint must validate identity + gesture gate.
        QDBusReply<QVariantMap> resolveMissingReply =
            iface.call(QStringLiteral("ResolveEmailBody"), QString(), QVariantMap{});
        QVERIFY(resolveMissingReply.isValid());
        const QVariantMap resolveMissingOut = resolveMissingReply.value();
        QVERIFY(!resolveMissingOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(resolveMissingOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("ResolveEmailBody"));
        QCOMPARE(resolveMissingOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("invalid-argument"));

        QDBusReply<QVariantMap> resolveNoGestureReply =
            iface.call(QStringLiteral("ResolveEmailBody"),
                       QStringLiteral("mail-msg-42"),
                       QVariantMap{{QStringLiteral("initiatedByUserGesture"), false}});
        QVERIFY(resolveNoGestureReply.isValid());
        const QVariantMap resolveNoGestureOut = resolveNoGestureReply.value();
        QVERIFY(!resolveNoGestureOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(resolveNoGestureOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("ResolveEmailBody"));
        QCOMPARE(resolveNoGestureOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));
        QCOMPARE(resolveNoGestureOut.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("user-gesture-required"));

        QDBusReply<QVariantMap> resolveClipMissingReply =
            iface.call(QStringLiteral("ResolveClipboardResult"), QString(), QVariantMap{});
        QVERIFY(resolveClipMissingReply.isValid());
        const QVariantMap resolveClipMissingOut = resolveClipMissingReply.value();
        QVERIFY(!resolveClipMissingOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(resolveClipMissingOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("ResolveClipboardResult"));
        QCOMPARE(resolveClipMissingOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("invalid-argument"));

        QDBusReply<QVariantMap> resolveClipNoGestureReply =
            iface.call(QStringLiteral("ResolveClipboardResult"),
                       QStringLiteral("clip-result-1"),
                       QVariantMap{{QStringLiteral("initiatedByUserGesture"), false}});
        QVERIFY(resolveClipNoGestureReply.isValid());
        const QVariantMap resolveClipNoGestureOut = resolveClipNoGestureReply.value();
        QVERIFY(!resolveClipNoGestureOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(resolveClipNoGestureOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("ResolveClipboardResult"));
        QCOMPARE(resolveClipNoGestureOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));
        QCOMPARE(resolveClipNoGestureOut.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("user-gesture-required"));

        QDBusReply<QVariantMap> notifPreviewReply =
            iface.call(QStringLiteral("QueryNotificationHistoryPreview"),
                       QStringLiteral("urgent"),
                       QVariantMap{});
        QVERIFY(notifPreviewReply.isValid());
        const QVariantMap notifPreviewOut = notifPreviewReply.value();
        QVERIFY(!notifPreviewOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(notifPreviewOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("QueryNotificationHistoryPreview"));
        QCOMPARE(notifPreviewOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));

        QDBusReply<QVariantMap> notifReadNoGestureReply =
            iface.call(QStringLiteral("ReadNotificationHistoryItem"),
                       QStringLiteral("n-1"),
                       QVariantMap{{QStringLiteral("initiatedByUserGesture"), false}});
        QVERIFY(notifReadNoGestureReply.isValid());
        const QVariantMap notifReadNoGestureOut = notifReadNoGestureReply.value();
        QVERIFY(!notifReadNoGestureOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(notifReadNoGestureOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("ReadNotificationHistoryItem"));
        QCOMPARE(notifReadNoGestureOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));
        QCOMPARE(notifReadNoGestureOut.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("user-gesture-required"));
    }

    void searchSummaryQuery_contractAndPolicy()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FakeSearchService fakeSearch;
        if (!registerFakeSearch(fakeSearch)) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        fakeSearch.rows = {
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("file-1")},
                {QStringLiteral("metadata"), QVariantMap{
                     {QStringLiteral("provider"), QStringLiteral("tracker")},
                     {QStringLiteral("title"), QStringLiteral("notes.txt")},
                     {QStringLiteral("subtitle"), QStringLiteral("/home/garis/Documents")},
                     {QStringLiteral("icon"), QStringLiteral("text-plain")},
                     {QStringLiteral("resultType"), QStringLiteral("file")},
                     {QStringLiteral("timestamp"), QStringLiteral("2026-03-20T01:00:00Z")},
                 }},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("contact-1")},
                {QStringLiteral("metadata"), QVariantMap{
                     {QStringLiteral("provider"), QStringLiteral("contacts")},
                     {QStringLiteral("title"), QStringLiteral("Alice Doe")},
                     {QStringLiteral("subtitle"), QStringLiteral("alice@example.com")},
                     {QStringLiteral("icon"), QStringLiteral("avatar-default")},
                     {QStringLiteral("resultType"), QStringLiteral("contact")},
                 }},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("mail-1")},
                {QStringLiteral("metadata"), QVariantMap{
                     {QStringLiteral("provider"), QStringLiteral("email")},
                     {QStringLiteral("title"), QStringLiteral("Meeting agenda")},
                     {QStringLiteral("subtitle"), QStringLiteral("From Bob")},
                     {QStringLiteral("icon"), QStringLiteral("mail-message")},
                     {QStringLiteral("resultType"), QStringLiteral("email")},
                 }},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("event-1")},
                {QStringLiteral("metadata"), QVariantMap{
                     {QStringLiteral("provider"), QStringLiteral("calendar")},
                     {QStringLiteral("title"), QStringLiteral("Design review")},
                     {QStringLiteral("subtitle"), QStringLiteral("10:00")},
                     {QStringLiteral("icon"), QStringLiteral("calendar")},
                     {QStringLiteral("resultType"), QStringLiteral("event")},
                 }},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("clip-1")},
                {QStringLiteral("metadata"), QVariantMap{
                     {QStringLiteral("provider"), QStringLiteral("clipboard")},
                     {QStringLiteral("title"), QStringLiteral("token: 123456")},
                     {QStringLiteral("icon"), QStringLiteral("edit-paste")},
                     {QStringLiteral("isSensitive"), true},
                     {QStringLiteral("preview"), QVariantMap{
                         {QStringLiteral("contentType"), QStringLiteral("text")},
                         {QStringLiteral("preview"), QStringLiteral("token: 123456")},
                         {QStringLiteral("isSensitive"), true},
                         {QStringLiteral("timestampBucket"), 1742432400ll},
                     }},
                 }},
            },
        };

        FakePortalBackend backend;

        PortalManager manager;
        PortalService service(&manager, &backend);
        if (!service.serviceRegistered()) {
            QSKIP("cannot register service name on session bus (likely already owned)");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             m_bus);
        QVERIFY(iface.isValid());

        // Low-risk query allowed by default for third-party callers.
        QDBusReply<QVariantMap> filesReply = iface.call(QStringLiteral("QueryFiles"),
                                                        QStringLiteral("note"),
                                                        QVariantMap{});
        QVERIFY(filesReply.isValid());
        const QVariantMap filesOut = filesReply.value();
        QVERIFY(filesOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(filesOut.value(QStringLiteral("method")).toString(), QStringLiteral("QueryFiles"));
        const QVariantList fileRows = filesOut.value(QStringLiteral("results")).toList();
        QCOMPARE(fileRows.size(), 1);
        QCOMPARE(fileRows.first().toMap().value(QStringLiteral("provider")).toString(),
                 QStringLiteral("tracker"));

        // Clipboard search is summary-only and redacts sensitive preview text.
        QDBusReply<QVariantMap> clipReply = iface.call(QStringLiteral("QueryClipboardPreview"),
                                                       QStringLiteral("token"),
                                                       QVariantMap{});
        QVERIFY(clipReply.isValid());
        const QVariantMap clipOut = clipReply.value();
        QVERIFY(clipOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(clipOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("QueryClipboardPreview"));
        const QVariantList clipRows = clipOut.value(QStringLiteral("results")).toList();
        QCOMPARE(clipRows.size(), 1);
        const QVariantMap clipRow = clipRows.first().toMap();
        QCOMPARE(clipRow.value(QStringLiteral("provider")).toString(), QStringLiteral("clipboard"));
        QCOMPARE(clipRow.value(QStringLiteral("previewText")).toString(),
                 QStringLiteral("Sensitive item"));
        QVERIFY(clipRow.value(QStringLiteral("isSensitive")).toBool());

        // Medium/high-risk direct summary methods require mediation request flow.
        QDBusReply<QVariantMap> contactsReply = iface.call(QStringLiteral("QueryContacts"),
                                                           QStringLiteral("alice"),
                                                           QVariantMap{});
        QVERIFY(contactsReply.isValid());
        const QVariantMap contactsOut = contactsReply.value();
        QVERIFY(!contactsOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(contactsOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("mediation-required-use-request-flow"));

        QDBusReply<QVariantMap> emailReply = iface.call(QStringLiteral("QueryEmailMetadata"),
                                                        QStringLiteral("meeting"),
                                                        QVariantMap{});
        QVERIFY(emailReply.isValid());
        const QVariantMap emailOut = emailReply.value();
        QVERIFY(!emailOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(emailOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("mediation-required-use-request-flow"));

        QDBusReply<QVariantMap> calendarReply = iface.call(QStringLiteral("QueryCalendarEvents"),
                                                           QStringLiteral("review"),
                                                           QVariantMap{});
        QVERIFY(calendarReply.isValid());
        const QVariantMap calendarOut = calendarReply.value();
        QVERIFY(!calendarOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(calendarOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("mediation-required-use-request-flow"));
    }

    void actionsQuery_contract()
    {
        if (!m_bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        FakeCapabilityService fakeCaps;
        fakeCaps.actionRows = {
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("a-1")},
                {QStringLiteral("name"), QStringLiteral("Resize to 50%")},
                {QStringLiteral("icon"), QStringLiteral("transform-scale")},
                {QStringLiteral("score"), 91},
                {QStringLiteral("intent"), QStringLiteral("resize-image")},
                {QStringLiteral("group"), QStringLiteral("Resize")},
            },
        };
        fakeCaps.contextRows = {
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("ctx-1")},
                {QStringLiteral("name"), QStringLiteral("Convert to WebP")},
                {QStringLiteral("icon"), QStringLiteral("image-webp")},
                {QStringLiteral("group"), QStringLiteral("Convert")},
            },
        };
        if (!registerFakeCapabilities(fakeCaps)) {
            QSKIP("org.freedesktop.SLMCapabilities already owned in this environment");
        }

        FakePortalBackend backend;
        PortalManager manager;
        PortalService service(&manager, &backend);
        if (!service.serviceRegistered()) {
            QSKIP("cannot register service name on session bus (likely already owned)");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             m_bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> queryActionsReply =
            iface.call(QStringLiteral("QueryActions"), QStringLiteral("resize"), QVariantMap{});
        QVERIFY(queryActionsReply.isValid());
        const QVariantMap queryActionsOut = queryActionsReply.value();
        QVERIFY(queryActionsOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(queryActionsOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("QueryActions"));
        QCOMPARE(queryActionsOut.value(QStringLiteral("count")).toInt(), 1);
        QCOMPARE(queryActionsOut.value(QStringLiteral("results")).toList().constFirst().toMap()
                     .value(QStringLiteral("id")).toString(),
                 QStringLiteral("a-1"));

        const QVariantList uris{QStringLiteral("file:///tmp/a.jpg")};
        QDBusReply<QVariantMap> queryContextReply =
            iface.call(QStringLiteral("QueryContextActions"),
                       QStringLiteral("file"),
                       uris,
                       QVariantMap{});
        QVERIFY(queryContextReply.isValid());
        const QVariantMap queryContextOut = queryContextReply.value();
        QVERIFY(queryContextOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(queryContextOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("QueryContextActions"));
        QCOMPARE(queryContextOut.value(QStringLiteral("count")).toInt(), 1);
        QCOMPARE(queryContextOut.value(QStringLiteral("results")).toList().constFirst().toMap()
                     .value(QStringLiteral("id")).toString(),
                 QStringLiteral("ctx-1"));

        QDBusReply<QVariantMap> invokeNoGestureReply =
            iface.call(QStringLiteral("InvokeAction"),
                       QStringLiteral("ctx-1"),
                       QVariantMap{{QStringLiteral("initiatedByUserGesture"), false}});
        QVERIFY(invokeNoGestureReply.isValid());
        const QVariantMap invokeNoGestureOut = invokeNoGestureReply.value();
        QVERIFY(!invokeNoGestureOut.value(QStringLiteral("ok")).toBool());
        QCOMPARE(invokeNoGestureOut.value(QStringLiteral("method")).toString(),
                 QStringLiteral("InvokeAction"));
        QCOMPARE(invokeNoGestureOut.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));
        QCOMPARE(invokeNoGestureOut.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("user-gesture-required"));
    }
};

QTEST_MAIN(PortalServiceDbusTest)
#include "portalservice_dbus_test.moc"
