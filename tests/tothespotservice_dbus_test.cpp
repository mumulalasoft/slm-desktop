#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "../tothespotservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Search.v1";
constexpr const char kPath[] = "/org/slm/Desktop/Search";

class FakeSearchService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Search.v1")

public slots:
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
};

QTEST_MAIN(TothespotServiceDbusTest)
#include "tothespotservice_dbus_test.moc"

