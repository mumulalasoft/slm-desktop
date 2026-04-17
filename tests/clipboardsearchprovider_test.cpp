#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QRegularExpression>
#include <QtTest>

#include "../src/services/clipboard/ClipboardSearchProvider.h"

namespace {
constexpr const char kService[] = "org.desktop.Clipboard";
constexpr const char kPath[] = "/org/desktop/Clipboard";

class FakeClipboardService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop.Clipboard")

public:
    QVariantList rows;

public slots:
    QVariantList GetHistory(int limit) const
    {
        if (limit <= 0 || rows.isEmpty()) {
            return {};
        }
        QVariantList out;
        for (int i = 0; i < rows.size() && i < limit; ++i) {
            out.push_back(rows.at(i));
        }
        return out;
    }

    QVariantList Search(const QString &, int limit) const
    {
        return GetHistory(limit);
    }
};

class ClipboardSearchProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void sensitiveExcluded_andPreviewNormalized()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }
        QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("dbus interface unavailable");
        }
        const QString service = QString::fromLatin1(kService);
        const QString path = QString::fromLatin1(kPath);
        if (iface->isServiceRegistered(service).value()) {
            QSKIP("org.desktop.Clipboard already registered");
        }

        FakeClipboardService fake;
        QVariantMap normal;
        normal.insert(QStringLiteral("id"), 1);
        normal.insert(QStringLiteral("type"), QStringLiteral("TEXT"));
        normal.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        normal.insert(QStringLiteral("preview"),
                      QStringLiteral("line1\nline2 with long tail that should be clipped because it exceeds "
                                     "the allowed preview width for provider output"));
        normal.insert(QStringLiteral("content"), QStringLiteral("normal content"));
        QVariantMap sensitive;
        sensitive.insert(QStringLiteral("id"), 2);
        sensitive.insert(QStringLiteral("type"), QStringLiteral("TEXT"));
        sensitive.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        sensitive.insert(QStringLiteral("preview"), QStringLiteral("temporary token"));
        sensitive.insert(QStringLiteral("content"), QStringLiteral("password=abc123"));
        fake.rows = {normal, sensitive};

        QVERIFY(bus.registerService(service));
        QVERIFY(bus.registerObject(path, &fake, QDBusConnection::ExportAllSlots));

        Slm::Clipboard::ClipboardSearchProvider provider;
        QVERIFY(provider.available());
        const QVariantList results = provider.query(QString(), 10);
        QCOMPARE(results.size(), 1);
        const QVariantMap first = results.constFirst().toMap();
        const QString title = first.value(QStringLiteral("title")).toString();
        QVERIFY(!title.contains('\n'));
        QVERIFY(title.size() <= 80);
        QCOMPARE(first.value(QStringLiteral("id")).toString(), QStringLiteral("clipboard:1"));
        const QVariantMap previewMap = first.value(QStringLiteral("preview")).toMap();
        QVERIFY(!previewMap.contains(QStringLiteral("content")));

        // includeSensitive=true must still be blocked without trusted+gesture.
        const QVariantList untrustedSensitive = provider.query(
            QString(),
            10,
            QVariantMap{{QStringLiteral("includeSensitive"), true},
                        {QStringLiteral("trustedCaller"), false},
                        {QStringLiteral("userGesture"), false}});
        QCOMPARE(untrustedSensitive.size(), 1);

        // Trusted + gesture can include sensitive item as redacted row.
        const QVariantList trustedSensitive = provider.query(
            QString(),
            10,
            QVariantMap{{QStringLiteral("includeSensitive"), true},
                        {QStringLiteral("trustedCaller"), true},
                        {QStringLiteral("userGesture"), true}});
        QCOMPARE(trustedSensitive.size(), 2);
        bool sawRedacted = false;
        for (const QVariant &rowVar : trustedSensitive) {
            const QVariantMap row = rowVar.toMap();
            if (row.value(QStringLiteral("isSensitive")).toBool()) {
                sawRedacted = true;
                QCOMPARE(row.value(QStringLiteral("title")).toString(), QStringLiteral("Sensitive item"));
            }
        }
        QVERIFY(sawRedacted);

        bus.unregisterObject(path);
        bus.unregisterService(service);
    }
};

} // namespace

QTEST_MAIN(ClipboardSearchProviderTest)
#include "clipboardsearchprovider_test.moc"
