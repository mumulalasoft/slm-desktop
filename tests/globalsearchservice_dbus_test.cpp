#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDateTime>
#include <QHash>
#include <QSignalSpy>

#include "../appmodel.h"
#include "../globalsearchservice.h"
#include "../src/core/workspace/spacesmanager.h"
#include "../src/core/workspace/windowingbackendmanager.h"
#include "../src/core/workspace/workspacemanager.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Search.v1";
constexpr const char kPath[] = "/org/slm/Desktop/Search";
constexpr const char kIface[] = "org.slm.Desktop.Search.v1";

class FakeClipboardService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop.Clipboard")

public:
public slots:
    QVariantList GetHistory(int) const
    {
        return {
            QVariantMap{
                {QStringLiteral("id"), 10},
                {QStringLiteral("type"), QStringLiteral("TEXT")},
                {QStringLiteral("content"), QStringLiteral("docker run -it ubuntu")},
                {QStringLiteral("preview"), QStringLiteral("docker run -it ubuntu")},
                {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()},
                {QStringLiteral("source_app"), QStringLiteral("org.example.Terminal")},
                {QStringLiteral("isPinned"), false},
            },
        };
    }

    QVariantList Search(const QString &query, int) const
    {
        if (query.trimmed().contains(QStringLiteral("docker"), Qt::CaseInsensitive)) {
            return GetHistory(10);
        }
        return {};
    }

    bool PasteItem(qlonglong id)
    {
        m_lastPasteId = id;
        ++m_pasteCount;
        return id == 10;
    }

    qlonglong lastPasteId() const { return m_lastPasteId; }
    int pasteCount() const { return m_pasteCount; }

private:
    qlonglong m_lastPasteId = -1;
    int m_pasteCount = 0;
};

class FakeSettingsSearchService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Settings.Search.v1")

public slots:
    SearchResultList Query(const QString &text, const QVariantMap &options) const
    {
        Q_UNUSED(options)
        SearchResultList out;
        if (!text.contains(QStringLiteral("print"), Qt::CaseInsensitive)) {
            return out;
        }
        SearchResultEntry entry;
        entry.id = QStringLiteral("setting:print/pdf-fallback-printer");
        entry.metadata = QVariantMap{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("provider"), QStringLiteral("settings")},
            {QStringLiteral("type"), QStringLiteral("setting")},
            {QStringLiteral("moduleId"), QStringLiteral("print")},
            {QStringLiteral("settingId"), QStringLiteral("pdf-fallback-printer")},
            {QStringLiteral("title"), QStringLiteral("PDF Fallback Printer")},
            {QStringLiteral("subtitle"), QStringLiteral("Print - Preferred virtual PDF printer")},
            {QStringLiteral("icon"), QStringLiteral("printer")},
            {QStringLiteral("action"), QStringLiteral("settings://print/pdf-fallback-printer")},
            {QStringLiteral("score"), 320},
        };
        out.push_back(entry);
        return out;
    }

    void Activate(const QString &id)
    {
        Q_UNUSED(id)
    }
};

class FakeDesktopSettings : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QVariant settingValue(const QString &path,
                                      const QVariant &fallback = QVariant()) const
    {
        const QString key = path.trimmed();
        return m_values.contains(key) ? m_values.value(key) : fallback;
    }

    Q_INVOKABLE bool setSettingValue(const QString &path, const QVariant &value)
    {
        const QString key = path.trimmed();
        if (key.isEmpty()) {
            return false;
        }
        if (m_values.value(key) == value) {
            return false;
        }
        m_values.insert(key, value);
        emit settingChanged(key);
        return true;
    }

signals:
    void settingChanged(const QString &path);

private:
    QHash<QString, QVariant> m_values;
};
}

class GlobalSearchServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void searchProfiles_includeSettingsBoost_contract()
    {
        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        FakeDesktopSettings settings;
        DesktopAppModel appModel;
        appModel.setDesktopSettings(&settings);
        appModel.refresh();

        GlobalSearchService service(&appModel, &workspace, &settings);
        const QVariantList profiles = service.GetSearchProfiles();
        QVERIFY(!profiles.isEmpty());

        auto boostFor = [&](const QString &profileId) -> QVariantMap {
            for (const QVariant &v : profiles) {
                const QVariantMap row = v.toMap();
                if (row.value(QStringLiteral("id")).toString() == profileId) {
                    return row.value(QStringLiteral("sourceBoost")).toMap();
                }
            }
            return {};
        };

        const QVariantMap balanced = boostFor(QStringLiteral("balanced"));
        const QVariantMap appsFirst = boostFor(QStringLiteral("apps-first"));
        const QVariantMap filesFirst = boostFor(QStringLiteral("files-first"));
        QVERIFY(!balanced.isEmpty());
        QVERIFY(!appsFirst.isEmpty());
        QVERIFY(!filesFirst.isEmpty());
        QCOMPARE(balanced.value(QStringLiteral("settings")).toInt(), 12);
        QCOMPARE(appsFirst.value(QStringLiteral("settings")).toInt(), 18);
        QCOMPARE(filesFirst.value(QStringLiteral("settings")).toInt(), 4);
    }

    void querySettingsProvider_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        QDBusConnectionInterface *busIface = bus.interface();
        QVERIFY(busIface);
        if (busIface->isServiceRegistered(QStringLiteral("org.slm.Settings.Search")).value()) {
            QSKIP("org.slm.Settings.Search already owned in this environment");
        }
        FakeSettingsSearchService fakeSettings;
        QVERIFY(bus.registerService(QStringLiteral("org.slm.Settings.Search")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/slm/Settings/Search"),
                                   &fakeSettings,
                                   QDBusConnection::ExportAllSlots));

        qDBusRegisterMetaType<SearchResultEntry>();
        qDBusRegisterMetaType<SearchResultList>();

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        FakeDesktopSettings settings;
        DesktopAppModel appModel;
        appModel.setDesktopSettings(&settings);
        appModel.refresh();

        GlobalSearchService service(&appModel, &workspace, &settings);
        if (!service.serviceRegistered()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        const SearchResultList localRows = service.Query(
            QStringLiteral("print"),
            QVariantMap{
                {QStringLiteral("limit"), 10},
                {QStringLiteral("includeApps"), false},
                {QStringLiteral("includeRecent"), false},
                {QStringLiteral("includeTracker"), false},
                {QStringLiteral("includeActions"), false},
                {QStringLiteral("includeClipboard"), false},
                {QStringLiteral("includeSettings"), true},
                {QStringLiteral("initiatedFromOfficialUI"), true},
                {QStringLiteral("initiatedByUserGesture"), true},
            });
        QVERIFY(!localRows.isEmpty());
        const SearchResultEntry first = localRows.first();
        QCOMPARE(first.metadata.value(QStringLiteral("provider")).toString(),
                 QStringLiteral("settings"));
        QCOMPARE(first.metadata.value(QStringLiteral("moduleId")).toString(),
                 QStringLiteral("print"));
        QVERIFY(first.metadata.value(QStringLiteral("deepLink"),
                                     first.metadata.value(QStringLiteral("action")))
                    .toString()
                    .startsWith(QStringLiteral("settings://print")));

        bus.unregisterObject(QStringLiteral("/org/slm/Settings/Search"));
        bus.unregisterService(QStringLiteral("org.slm.Settings.Search"));
    }

    void queryResolveClipboard_boundary_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        // Register fake clipboard backend for deterministic integration test.
        QDBusConnectionInterface *busIface = bus.interface();
        QVERIFY(busIface);
        if (busIface->isServiceRegistered(QStringLiteral("org.desktop.Clipboard")).value()) {
            QSKIP("org.desktop.Clipboard already owned in this environment");
        }
        FakeClipboardService fakeClipboard;
        QVERIFY(bus.registerService(QStringLiteral("org.desktop.Clipboard")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/desktop/Clipboard"),
                                   &fakeClipboard,
                                   QDBusConnection::ExportAllSlots));

        qDBusRegisterMetaType<SearchResultEntry>();
        qDBusRegisterMetaType<SearchResultList>();

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        FakeDesktopSettings settings;
        DesktopAppModel appModel;
        appModel.setDesktopSettings(&settings);
        appModel.refresh();

        GlobalSearchService service(&appModel, &workspace, &settings);
        if (!service.serviceRegistered()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        // Local in-process query path: validates summary contract from clipboard provider.
        const SearchResultList localRows = service.Query(
            QStringLiteral("docker"),
            QVariantMap{
                {QStringLiteral("limit"), 10},
                {QStringLiteral("includeApps"), false},
                {QStringLiteral("includeRecent"), false},
                {QStringLiteral("includeTracker"), false},
                {QStringLiteral("includeActions"), false},
                {QStringLiteral("includeClipboard"), true},
                {QStringLiteral("initiatedFromOfficialUI"), true},
                {QStringLiteral("initiatedByUserGesture"), true},
            });
        QVERIFY(!localRows.isEmpty());
        const SearchResultEntry first = localRows.first();
        QCOMPARE(first.metadata.value(QStringLiteral("provider")).toString(),
                 QStringLiteral("clipboard"));
        QVERIFY(!first.metadata.value(QStringLiteral("clipboardId")).toString().isEmpty()
                || first.metadata.value(QStringLiteral("clipboardId")).toLongLong() > 0);
        QVERIFY(first.metadata.contains(QStringLiteral("preview")));

        // Resolve must be denied without explicit user gesture.
        const QVariantMap denied =
            service.ResolveClipboardResult(first.id,
                                           QVariantMap{
                                               {QStringLiteral("initiatedFromOfficialUI"), true},
                                               {QStringLiteral("initiatedByUserGesture"), false},
                                           });
        QCOMPARE(denied.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(denied.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));
        QCOMPARE(fakeClipboard.pasteCount(), 0);

        // Resolve is allowed with explicit gesture and triggers recopy path.
        const QVariantMap allowed =
            service.ResolveClipboardResult(first.id,
                                           QVariantMap{
                                               {QStringLiteral("initiatedFromOfficialUI"), true},
                                               {QStringLiteral("initiatedByUserGesture"), true},
                                               {QStringLiteral("recopyBeforePaste"), true},
                                           });
        QCOMPARE(allowed.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(allowed.value(QStringLiteral("provider")).toString(),
                 QStringLiteral("clipboard"));
        QCOMPARE(fakeClipboard.pasteCount(), 1);
        QCOMPARE(fakeClipboard.lastPasteId(), 10);

        bus.unregisterObject(QStringLiteral("/org/desktop/Clipboard"));
        bus.unregisterService(QStringLiteral("org.desktop.Clipboard"));
    }

    void resolveClipboardResult_dbusCallerDenied_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        QDBusConnectionInterface *busIface = bus.interface();
        QVERIFY(busIface);
        if (busIface->isServiceRegistered(QStringLiteral("org.desktop.Clipboard")).value()) {
            QSKIP("org.desktop.Clipboard already owned in this environment");
        }
        FakeClipboardService fakeClipboard;
        QVERIFY(bus.registerService(QStringLiteral("org.desktop.Clipboard")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/desktop/Clipboard"),
                                   &fakeClipboard,
                                   QDBusConnection::ExportAllSlots));

        qDBusRegisterMetaType<SearchResultEntry>();
        qDBusRegisterMetaType<SearchResultList>();

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        FakeDesktopSettings settings;
        DesktopAppModel appModel;
        appModel.setDesktopSettings(&settings);
        appModel.refresh();

        GlobalSearchService service(&appModel, &workspace, &settings);
        if (!service.serviceRegistered()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QVariantMap options;
        options.insert(QStringLiteral("limit"), 10);
        options.insert(QStringLiteral("includeApps"), false);
        options.insert(QStringLiteral("includeRecent"), false);
        options.insert(QStringLiteral("includeTracker"), false);
        options.insert(QStringLiteral("includeActions"), false);
        options.insert(QStringLiteral("includeClipboard"), true);
        options.insert(QStringLiteral("initiatedFromOfficialUI"), true);
        options.insert(QStringLiteral("initiatedByUserGesture"), true);

        QDBusReply<SearchResultList> queryReply = iface.call(QStringLiteral("Query"),
                                                             QStringLiteral("docker"),
                                                             options);
        QVERIFY(queryReply.isValid());
        const SearchResultList rows = queryReply.value();
        QVERIFY(!rows.isEmpty());
        const QString firstId = rows.first().id;
        QVERIFY(!firstId.isEmpty());

        // DBus caller is not a trusted shell component in this test context.
        QDBusReply<QVariantMap> resolveReply =
            iface.call(QStringLiteral("ResolveClipboardResult"),
                       firstId,
                       QVariantMap{
                           {QStringLiteral("initiatedFromOfficialUI"), true},
                           {QStringLiteral("initiatedByUserGesture"), true},
                           {QStringLiteral("recopyBeforePaste"), true},
                       });
        QVERIFY(resolveReply.isValid());
        const QVariantMap resolved = resolveReply.value();
        QCOMPARE(resolved.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(resolved.value(QStringLiteral("error")).toString(),
                 QStringLiteral("permission-denied"));
        QCOMPARE(fakeClipboard.pasteCount(), 0);

        bus.unregisterObject(QStringLiteral("/org/desktop/Clipboard"));
        bus.unregisterService(QStringLiteral("org.desktop.Clipboard"));
    }

    void methodsAndSignals_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        qDBusRegisterMetaType<SearchResultEntry>();
        qDBusRegisterMetaType<SearchResultList>();

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        FakeDesktopSettings settings;
        DesktopAppModel appModel;
        appModel.setDesktopSettings(&settings);
        appModel.refresh();

        GlobalSearchService service(&appModel, &workspace, &settings);
        if (!service.serviceRegistered()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QSignalSpy indexingStartedSpy(&service, SIGNAL(IndexingStarted()));
        QSignalSpy indexingFinishedSpy(&service, SIGNAL(IndexingFinished()));
        QSignalSpy providerSpy(&service, SIGNAL(ProviderRegistered(QString)));
        QSignalSpy profileChangedSpy(&service, SIGNAL(SearchProfileChanged(QString)));
        QVERIFY(indexingStartedSpy.isValid());
        QVERIFY(indexingFinishedSpy.isValid());
        QVERIFY(providerSpy.isValid());
        QVERIFY(profileChangedSpy.isValid());

        QTRY_VERIFY_WITH_TIMEOUT(providerSpy.count() >= 1, 1000);
        bool sawApps = false;
        bool sawRecent = false;
        bool sawSettings = false;
        for (int i = 0; i < providerSpy.count(); ++i) {
            const QString provider = providerSpy.at(i).at(0).toString();
            if (provider == QStringLiteral("apps")) {
                sawApps = true;
            } else if (provider == QStringLiteral("recent_files")) {
                sawRecent = true;
            } else if (provider == QStringLiteral("settings")) {
                sawSettings = true;
            }
        }
        QVERIFY(sawApps);
        QVERIFY(sawRecent);
        QVERIFY(sawSettings);

        QVariantMap options;
        options.insert(QStringLiteral("limit"), 10);
        options.insert(QStringLiteral("includeApps"), true);
        options.insert(QStringLiteral("includePreview"), true);

        QDBusReply<SearchResultList> queryReply = iface.call(QStringLiteral("Query"),
                                                             QStringLiteral("a"),
                                                             options);
        QVERIFY(queryReply.isValid());
        const SearchResultList results = queryReply.value();
        QVERIFY(results.size() <= 10);
        QTRY_VERIFY_WITH_TIMEOUT(indexingStartedSpy.count() >= 1, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(indexingFinishedSpy.count() >= 1, 1000);

        if (results.isEmpty()) {
            QSKIP("app index is empty in this test environment");
        }

        const SearchResultEntry first = results.first();
        QVERIFY(!first.id.trimmed().isEmpty());
        QVERIFY(!first.metadata.value(QStringLiteral("title")).toString().trimmed().isEmpty());
        QVERIFY(first.metadata.contains(QStringLiteral("score")));
        QVERIFY(first.metadata.contains(QStringLiteral("preview")));

        QDBusReply<QVariantMap> previewReply = iface.call(QStringLiteral("PreviewResult"), first.id);
        QVERIFY(previewReply.isValid());
        const QVariantMap preview = previewReply.value();
        QVERIFY(preview.value(QStringLiteral("ok")).toBool());
        QCOMPARE(preview.value(QStringLiteral("id")).toString(), first.id);

        const QVariantMap activateData{
            {QStringLiteral("query"), QStringLiteral("alpha")},
            {QStringLiteral("provider"), first.metadata.value(QStringLiteral("provider")).toString()},
            {QStringLiteral("result_type"), QStringLiteral("app")},
            {QStringLiteral("action"), QStringLiteral("launch")},
        };
        QDBusReply<void> activateReply = iface.call(QStringLiteral("ActivateResult"),
                                                    first.id,
                                                    activateData);
        QVERIFY(activateReply.isValid());

        QDBusReply<QVariantList> telemetryReply =
            iface.call(QStringLiteral("GetActivationTelemetry"), 10);
        QVERIFY(telemetryReply.isValid());
        const QVariantList telemetry = telemetryReply.value();
        QVERIFY(!telemetry.isEmpty());
        const QVariantMap telemetryRow = telemetry.last().toMap();
        QCOMPARE(telemetryRow.value(QStringLiteral("id")).toString(), first.id);
        QCOMPARE(telemetryRow.value(QStringLiteral("query")).toString(), QStringLiteral("alpha"));
        QCOMPARE(telemetryRow.value(QStringLiteral("result_type")).toString(), QStringLiteral("app"));
        QCOMPARE(telemetryRow.value(QStringLiteral("action")).toString(), QStringLiteral("launch"));
        QVERIFY(!telemetryRow.value(QStringLiteral("provider")).toString().isEmpty());
        QVERIFY(!telemetryRow.value(QStringLiteral("timestampUtc")).toString().isEmpty());

        // Payload contract checks for file/folder activations.
        QDBusReply<void> fileActivateReply = iface.call(
            QStringLiteral("ActivateResult"),
            QStringLiteral("dummy:file"),
            QVariantMap{
                {QStringLiteral("query"), QStringLiteral("doc")},
                {QStringLiteral("provider"), QStringLiteral("recent_files")},
                {QStringLiteral("result_type"), QStringLiteral("file")},
                {QStringLiteral("action"), QStringLiteral("open")},
            });
        QVERIFY(fileActivateReply.isValid());

        QDBusReply<void> folderActivateReply = iface.call(
            QStringLiteral("ActivateResult"),
            QStringLiteral("dummy:folder"),
            QVariantMap{
                {QStringLiteral("query"), QStringLiteral("src")},
                {QStringLiteral("provider"), QStringLiteral("tracker")},
                {QStringLiteral("result_type"), QStringLiteral("folder")},
                {QStringLiteral("action"), QStringLiteral("open")},
            });
        QVERIFY(folderActivateReply.isValid());

        QDBusReply<QVariantList> telemetryTripletReply =
            iface.call(QStringLiteral("GetActivationTelemetry"), 3);
        QVERIFY(telemetryTripletReply.isValid());
        const QVariantList telemetryTriplet = telemetryTripletReply.value();
        QCOMPARE(telemetryTriplet.size(), 3);
        QCOMPARE(telemetryTriplet.at(0).toMap().value(QStringLiteral("result_type")).toString(),
                 QStringLiteral("app"));
        QCOMPARE(telemetryTriplet.at(1).toMap().value(QStringLiteral("result_type")).toString(),
                 QStringLiteral("file"));
        QCOMPARE(telemetryTriplet.at(2).toMap().value(QStringLiteral("result_type")).toString(),
                 QStringLiteral("folder"));
        QCOMPARE(telemetryTriplet.at(1).toMap().value(QStringLiteral("provider")).toString(),
                 QStringLiteral("recent_files"));
        QCOMPARE(telemetryTriplet.at(2).toMap().value(QStringLiteral("provider")).toString(),
                 QStringLiteral("tracker"));

        QDBusReply<bool> resetTelemetryReply = iface.call(QStringLiteral("ResetActivationTelemetry"));
        QVERIFY(resetTelemetryReply.isValid());
        QVERIFY(resetTelemetryReply.value());

        QDBusReply<QVariantList> telemetryAfterResetReply =
            iface.call(QStringLiteral("GetActivationTelemetry"), 10);
        QVERIFY(telemetryAfterResetReply.isValid());
        QVERIFY(telemetryAfterResetReply.value().isEmpty());

        // Ring buffer hardening: ensure cap and eviction order are stable.
        constexpr int kTelemetryBurst = 300;
        for (int i = 0; i < kTelemetryBurst; ++i) {
            const QString id = QStringLiteral("dummy:%1").arg(i);
            const QVariantMap burstData{
                {QStringLiteral("query"), QStringLiteral("burst")},
                {QStringLiteral("provider"), QStringLiteral("apps")},
                {QStringLiteral("result_type"), QStringLiteral("app")},
                {QStringLiteral("action"), QStringLiteral("launch")},
            };
            QDBusReply<void> burstReply = iface.call(QStringLiteral("ActivateResult"), id, burstData);
            QVERIFY(burstReply.isValid());
        }

        QDBusReply<QVariantMap> telemetryMetaReply =
            iface.call(QStringLiteral("GetTelemetryMeta"));
        QVERIFY(telemetryMetaReply.isValid());
        const QVariantMap telemetryMeta = telemetryMetaReply.value();
        QVERIFY(telemetryMeta.value(QStringLiteral("ok")).toBool());
        const int activationCapacity = telemetryMeta.value(QStringLiteral("activationCapacity")).toInt();
        QVERIFY(activationCapacity > 0);
        QVERIFY(kTelemetryBurst > activationCapacity);

        QDBusReply<QVariantList> telemetryBurstReply =
            iface.call(QStringLiteral("GetActivationTelemetry"), 1000);
        QVERIFY(telemetryBurstReply.isValid());
        const QVariantList burstRows = telemetryBurstReply.value();
        QCOMPARE(burstRows.size(), activationCapacity);
        QCOMPARE(burstRows.first().toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("dummy:%1").arg(kTelemetryBurst - activationCapacity));
        QCOMPARE(burstRows.last().toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("dummy:%1").arg(kTelemetryBurst - 1));

        QDBusReply<QVariantMap> cfgReply = iface.call(QStringLiteral("ConfigureTrackerPreset"), QVariantMap{});
        QVERIFY(cfgReply.isValid());
        QVERIFY(cfgReply.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(cfgReply.value().value(QStringLiteral("cpuLimit")).toInt(), 15);
        QCOMPARE(cfgReply.value().value(QStringLiteral("initialDelaySec")).toInt(), 120);

        QDBusReply<QVariantMap> statusReply = iface.call(QStringLiteral("TrackerPresetStatus"));
        QVERIFY(statusReply.isValid());
        QVERIFY(statusReply.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(!statusReply.value().value(QStringLiteral("path")).toString().isEmpty());

        QDBusReply<QVariantMap> resetReply = iface.call(QStringLiteral("ResetTrackerPreset"));
        QVERIFY(resetReply.isValid());
        QVERIFY(resetReply.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(resetReply.value().value(QStringLiteral("reset")).toBool(), true);

        QDBusReply<QVariantList> profilesReply = iface.call(QStringLiteral("GetSearchProfiles"));
        QVERIFY(profilesReply.isValid());
        const QVariantList profiles = profilesReply.value();
        QVERIFY(!profiles.isEmpty());
        bool hasBalanced = false;
        for (const QVariant &entry : profiles) {
            const QVariantMap row = entry.toMap();
            if (row.value(QStringLiteral("id")).toString() == QStringLiteral("balanced")) {
                hasBalanced = true;
                QVERIFY(row.value(QStringLiteral("sourceBoost")).toMap().contains(QStringLiteral("apps")));
                break;
            }
        }
        QVERIFY(hasBalanced);

        QDBusReply<QString> activeProfileReply = iface.call(QStringLiteral("GetActiveSearchProfile"));
        QVERIFY(activeProfileReply.isValid());
        QCOMPARE(activeProfileReply.value(), QStringLiteral("balanced"));

        QDBusReply<QVariantMap> activeProfileMetaReply =
            iface.call(QStringLiteral("GetActiveSearchProfileMeta"));
        QVERIFY(activeProfileMetaReply.isValid());
        const QVariantMap activeProfileMeta = activeProfileMetaReply.value();
        QVERIFY(activeProfileMeta.value(QStringLiteral("ok")).toBool());
        QCOMPARE(activeProfileMeta.value(QStringLiteral("profileId")).toString(), QStringLiteral("balanced"));
        QVERIFY(!activeProfileMeta.value(QStringLiteral("updatedAtUtc")).toString().isEmpty());
        QCOMPARE(activeProfileMeta.value(QStringLiteral("source")).toString(), QStringLiteral("desktopsettings"));

        QDBusReply<bool> setAppsFirstReply = iface.call(QStringLiteral("SetActiveSearchProfile"),
                                                        QStringLiteral("apps-first"));
        QVERIFY(setAppsFirstReply.isValid());
        QVERIFY(setAppsFirstReply.value());
        QTRY_VERIFY_WITH_TIMEOUT(profileChangedSpy.count() >= 1, 1000);
        QCOMPARE(profileChangedSpy.last().at(0).toString(), QStringLiteral("apps-first"));

        QDBusReply<QString> activeAppsFirstReply = iface.call(QStringLiteral("GetActiveSearchProfile"));
        QVERIFY(activeAppsFirstReply.isValid());
        QCOMPARE(activeAppsFirstReply.value(), QStringLiteral("apps-first"));

        QVariantMap appOnlyImplicitProfile;
        appOnlyImplicitProfile.insert(QStringLiteral("limit"), 1);
        appOnlyImplicitProfile.insert(QStringLiteral("includeApps"), true);
        appOnlyImplicitProfile.insert(QStringLiteral("includeRecent"), false);
        appOnlyImplicitProfile.insert(QStringLiteral("includeTracker"), false);
        QDBusReply<SearchResultList> implicitAppsFirstReply = iface.call(QStringLiteral("Query"),
                                                                         QStringLiteral("a"),
                                                                         appOnlyImplicitProfile);
        QVERIFY(implicitAppsFirstReply.isValid());
        if (!implicitAppsFirstReply.value().isEmpty()) {
            const int implicitScore =
                implicitAppsFirstReply.value().first().metadata.value(QStringLiteral("score")).toInt();
            QVariantMap appOnlyBalancedExplicit = appOnlyImplicitProfile;
            appOnlyBalancedExplicit.insert(QStringLiteral("searchProfile"), QStringLiteral("balanced"));
            QDBusReply<SearchResultList> balancedExplicitReply = iface.call(QStringLiteral("Query"),
                                                                            QStringLiteral("a"),
                                                                            appOnlyBalancedExplicit);
            QVERIFY(balancedExplicitReply.isValid());
            QVERIFY(!balancedExplicitReply.value().isEmpty());
            const int balancedExplicitScore =
                balancedExplicitReply.value().first().metadata.value(QStringLiteral("score")).toInt();
            // apps-first boosts apps by +40, balanced by +20 => implicit should be 20 higher.
            QCOMPARE(implicitScore - balancedExplicitScore, 20);
        }

        QDBusReply<bool> setInvalidReply = iface.call(QStringLiteral("SetActiveSearchProfile"),
                                                      QStringLiteral("invalid-profile"));
        QVERIFY(setInvalidReply.isValid());
        QVERIFY(setInvalidReply.value());
        QDBusReply<QString> activeAfterInvalidReply = iface.call(QStringLiteral("GetActiveSearchProfile"));
        QVERIFY(activeAfterInvalidReply.isValid());
        QCOMPARE(activeAfterInvalidReply.value(), QStringLiteral("balanced"));

        // Validate searchProfile affects score calculation.
        settings.setSettingValue(QStringLiteral("tothespot.searchProfile"), QStringLiteral("balanced"));
        QVariantMap appOnlyBalanced;
        appOnlyBalanced.insert(QStringLiteral("limit"), 1);
        appOnlyBalanced.insert(QStringLiteral("includeApps"), true);
        appOnlyBalanced.insert(QStringLiteral("includeRecent"), false);
        appOnlyBalanced.insert(QStringLiteral("includeTracker"), false);
        appOnlyBalanced.insert(QStringLiteral("searchProfile"), QStringLiteral("balanced"));
        QDBusReply<SearchResultList> balancedReply = iface.call(QStringLiteral("Query"),
                                                                QStringLiteral("a"),
                                                                appOnlyBalanced);
        QVERIFY(balancedReply.isValid());
        if (balancedReply.value().isEmpty()) {
            QSKIP("no app results available for profile score comparison");
        }
        const int balancedScore =
            balancedReply.value().first().metadata.value(QStringLiteral("score")).toInt();

        QVariantMap appOnlyFilesFirst = appOnlyBalanced;
        appOnlyFilesFirst.insert(QStringLiteral("searchProfile"), QStringLiteral("files-first"));
        QDBusReply<SearchResultList> filesFirstReply = iface.call(QStringLiteral("Query"),
                                                                  QStringLiteral("a"),
                                                                  appOnlyFilesFirst);
        QVERIFY(filesFirstReply.isValid());
        QVERIFY(!filesFirstReply.value().isEmpty());
        const int filesFirstScore =
            filesFirstReply.value().first().metadata.value(QStringLiteral("score")).toInt();

        // apps boost in balanced is +20, in files-first is +5 => delta 15.
        QCOMPARE(balancedScore - filesFirstScore, 15);
    }

    void activateResult_capabilitySplit_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        qDBusRegisterMetaType<SearchResultEntry>();
        qDBusRegisterMetaType<SearchResultList>();

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        FakeDesktopSettings settings;
        DesktopAppModel appModel;
        appModel.setDesktopSettings(&settings);
        appModel.refresh();

        GlobalSearchService service(&appModel, &workspace, &settings);
        if (!service.serviceRegistered()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<bool> resetReply = iface.call(QStringLiteral("ResetActivationTelemetry"));
        QVERIFY(resetReply.isValid());
        QVERIFY(resetReply.value());

        QDBusReply<void> appActivateReply = iface.call(
            QStringLiteral("ActivateResult"),
            QStringLiteral("split:app"),
            QVariantMap{
                {QStringLiteral("query"), QStringLiteral("calc")},
                {QStringLiteral("provider"), QStringLiteral("apps")},
                {QStringLiteral("result_type"), QStringLiteral("app")},
                {QStringLiteral("action"), QStringLiteral("launch")},
                {QStringLiteral("initiatedByUserGesture"), true},
            });
        QVERIFY(appActivateReply.isValid());

        QDBusReply<void> fileActivateReply = iface.call(
            QStringLiteral("ActivateResult"),
            QStringLiteral("split:file"),
            QVariantMap{
                {QStringLiteral("query"), QStringLiteral("readme")},
                {QStringLiteral("provider"), QStringLiteral("tracker")},
                {QStringLiteral("result_type"), QStringLiteral("file")},
                {QStringLiteral("action"), QStringLiteral("open")},
                {QStringLiteral("initiatedByUserGesture"), true},
            });
        QVERIFY(fileActivateReply.isValid());

        QDBusReply<void> clipboardActivateReply = iface.call(
            QStringLiteral("ActivateResult"),
            QStringLiteral("split:clipboard"),
            QVariantMap{
                {QStringLiteral("query"), QStringLiteral("token")},
                {QStringLiteral("provider"), QStringLiteral("clipboard")},
                {QStringLiteral("result_type"), QStringLiteral("file")},
                {QStringLiteral("action"), QStringLiteral("open")},
                {QStringLiteral("initiatedByUserGesture"), true},
            });
        QVERIFY(clipboardActivateReply.isValid());

        QDBusReply<QVariantList> telemetryReply =
            iface.call(QStringLiteral("GetActivationTelemetry"), 10);
        QVERIFY(telemetryReply.isValid());
        const QVariantList rows = telemetryReply.value();

        // Clipboard activation should be denied for third-party caller, so only app+file persist.
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("split:app"));
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("split:file"));
    }
};

QTEST_MAIN(GlobalSearchServiceDbusTest)
#include "globalsearchservice_dbus_test.moc"
