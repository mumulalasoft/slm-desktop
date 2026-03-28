#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QStandardPaths>

#include "../tothespotservice.h"
#include "../globalsearchservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Search.v1";
constexpr const char kPath[] = "/org/slm/Desktop/Search";

class FakeSearchService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Search.v1")

public slots:
    SearchResultList Query(const QString &text, const QVariantMap &options)
    {
        Q_UNUSED(options)
        SearchResultEntry row;
        row.id = QStringLiteral("app:com.slm.CachedApp.desktop");
        row.metadata = {
            {QStringLiteral("provider"), QStringLiteral("apps")},
            {QStringLiteral("title"), text.isEmpty() ? QStringLiteral("Cached App")
                                                     : QStringLiteral("Query App")},
            {QStringLiteral("subtitle"), QStringLiteral("Cached from fake search service")},
            {QStringLiteral("path"), QStringLiteral("com.slm.CachedApp.desktop")},
            {QStringLiteral("desktopId"), QStringLiteral("com.slm.CachedApp.desktop")},
            {QStringLiteral("icon"), QStringLiteral("application-x-executable-symbolic")},
            {QStringLiteral("score"), 250},
        };
        return {row};
    }

    QVariantMap ResolveClipboardResult(const QString &id, const QVariantMap &resolveData)
    {
        m_lastId = id;
        m_lastResolveData = resolveData;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("id"), id},
            {QStringLiteral("provider"), QStringLiteral("clipboard")},
            {QStringLiteral("clipboardId"), 42},
        };
    }

    QString lastId() const { return m_lastId; }
    QVariantMap lastResolveData() const { return m_lastResolveData; }

private:
    QString m_lastId;
    QVariantMap m_lastResolveData;
};
} // namespace

class TothespotServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qRegisterMetaType<SearchResultEntry>("SearchResultEntry");
        qRegisterMetaType<SearchResultList>("SearchResultList");
        qDBusRegisterMetaType<SearchResultEntry>();
        qDBusRegisterMetaType<SearchResultList>();
    }

    void resolveClipboardResult_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        QDBusConnectionInterface *iface = bus.interface();
        QVERIFY(iface);
        if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        FakeSearchService fake;
        QVERIFY(bus.registerService(QString::fromLatin1(kService)));
        QVERIFY(bus.registerObject(QString::fromLatin1(kPath),
                                   &fake,
                                   QDBusConnection::ExportAllSlots));

        TothespotService service;
        const QVariantMap req{
            {QStringLiteral("initiatedByUserGesture"), true},
            {QStringLiteral("initiatedFromOfficialUI"), true},
            {QStringLiteral("recopyBeforePaste"), true},
        };
        const QVariantMap out = service.resolveClipboardResult(QStringLiteral("clipboard:42"), req);
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("id")).toString(), QStringLiteral("clipboard:42"));
        QCOMPARE(out.value(QStringLiteral("provider")).toString(), QStringLiteral("clipboard"));
        QCOMPARE(out.value(QStringLiteral("clipboardId")).toInt(), 42);

        QCOMPARE(fake.lastId(), QStringLiteral("clipboard:42"));
        QCOMPARE(fake.lastResolveData().value(QStringLiteral("initiatedByUserGesture")).toBool(), true);
        QCOMPARE(fake.lastResolveData().value(QStringLiteral("initiatedFromOfficialUI")).toBool(), true);
        QCOMPARE(fake.lastResolveData().value(QStringLiteral("recopyBeforePaste")).toBool(), true);

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }

    void query_emptyQuery_cacheFallback_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        QDBusConnectionInterface *iface = bus.interface();
        QVERIFY(iface);
        if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        TothespotService service;
        FakeSearchService fake;
        QVERIFY(bus.registerService(QString::fromLatin1(kService)));
        QVERIFY(bus.registerObject(QString::fromLatin1(kPath),
                                   &fake,
                                   QDBusConnection::ExportAllSlots));

        const QVariantList warmRows = service.query(QString(), {}, 24);
        QVERIFY2(!warmRows.isEmpty(), "warm query should return at least one row");
        const QString warmName = warmRows.first().toMap().value(QStringLiteral("name")).toString();
        QVERIFY2(!warmName.trimmed().isEmpty(), "warm query row should have display name");

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));

        const QVariantList cachedRows = service.query(QString(), {}, 24);
        QVERIFY2(!cachedRows.isEmpty(), "cache fallback should return rows when backend is unavailable");
        const QString cachedName = cachedRows.first().toMap().value(QStringLiteral("name")).toString();
        QCOMPARE(cachedName, warmName);
    }

    void query_emptyQuery_cacheFallback_updatesTelemetry_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        QDBusConnectionInterface *iface = bus.interface();
        QVERIFY(iface);
        if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
            QSKIP("org.slm.Desktop.Search.v1 already owned in this environment");
        }

        TothespotService service;
        FakeSearchService fake;
        QVERIFY(bus.registerService(QString::fromLatin1(kService)));
        QVERIFY(bus.registerObject(QString::fromLatin1(kPath),
                                   &fake,
                                   QDBusConnection::ExportAllSlots));

        const QVariantList warmRows = service.query(QString(), {}, 24);
        QVERIFY2(!warmRows.isEmpty(), "warm query should return at least one row");

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));

        const QVariantList cachedRows = service.query(QString(), {}, 24);
        QVERIFY2(!cachedRows.isEmpty(), "cache fallback should still return rows");

        const QVariantMap telemetry = service.telemetryMeta();
        const QVariantMap providerHealth = telemetry.value(QStringLiteral("providerHealth")).toMap();
        QVERIFY2(!providerHealth.isEmpty(), "providerHealth should be present in telemetry");
        QCOMPARE(providerHealth.value(QStringLiteral("lastSource")).toString(), QStringLiteral("cache"));
        QCOMPARE(providerHealth.value(QStringLiteral("lastUsedFallback")).toBool(), true);
    }
};

QTEST_MAIN(TothespotServiceDbusTest)
#include "tothespotservice_dbus_test.moc"
