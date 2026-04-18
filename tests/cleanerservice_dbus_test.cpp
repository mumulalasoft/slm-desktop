#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "../src/services/cleaner/cleanerservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Cleaner";
constexpr const char kPath[] = "/org/slm/Desktop/Cleaner";
constexpr const char kIface[] = "org.slm.Desktop.Cleaner";

bool writeBytes(const QString &path, int size, char fill = 'x')
{
    QFileInfo info(path);
    QDir().mkpath(info.dir().absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    QByteArray payload(qMax(0, size), fill);
    return f.write(payload) == payload.size();
}
}

class CleanerServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_tempDir.isValid());
        const QString cache = m_tempDir.path() + QStringLiteral("/cache");
        const QString data = m_tempDir.path() + QStringLiteral("/data");
        qputenv("XDG_CACHE_HOME", cache.toUtf8());
        qputenv("XDG_DATA_HOME", data.toUtf8());

        QVERIFY(writeBytes(cache + "/thumbnails/normal/a.png", 1024));
        QVERIFY(writeBytes(cache + "/thumbnails/fail/b.png", 2048));
        QVERIFY(writeBytes(cache + "/thumbnails/normal/invalid.txt", 333)); // corrupted thumbnail candidate
        QVERIFY(writeBytes(cache + "/google-chrome/code cache/file.bin", 4096));
        QVERIFY(writeBytes(cache + "/mesa_shader_cache/cache.db", 1024));
    }

    void cleanup()
    {
        qputenv("XDG_CACHE_HOME", QByteArray());
        qputenv("XDG_DATA_HOME", QByteArray());
    }

    void ping_scan_clean_policy_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus unavailable");
        }
        QDBusConnectionInterface *ifaceBus = bus.interface();
        if (!ifaceBus) {
            QSKIP("dbus interface unavailable");
        }
        if (ifaceBus->isServiceRegistered(QString::fromLatin1(kService)).value()) {
            QSKIP("org.slm.Desktop.Cleaner already owned");
        }

        Slm::Cleaner::CleanerService service;
        QVERIFY(bus.registerService(QString::fromLatin1(kService)));
        QVERIFY(bus.registerObject(QString::fromLatin1(kPath),
                                   &service,
                                   QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> ping = iface.call(QStringLiteral("Ping"));
        QVERIFY(ping.isValid());
        QVERIFY(ping.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> scan = iface.call(QStringLiteral("Scan"));
        QVERIFY(scan.isValid());
        QVERIFY(scan.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(scan.value().value(QStringLiteral("totalSizeBytes")).toULongLong() > 0);

        QVariantMap previewOptions{
            {QStringLiteral("mode"), QStringLiteral("smart")},
            {QStringLiteral("days"), 1},
            {QStringLiteral("clearThumbnail"), true},
            {QStringLiteral("clearFailedThumbnail"), true},
            {QStringLiteral("selectedApps"), QStringList{
                 m_tempDir.path() + QStringLiteral("/cache/google-chrome")
             }}
        };
        QDBusReply<QVariantMap> preview = iface.call(QStringLiteral("PreviewClean"), previewOptions);
        QVERIFY(preview.isValid());
        QVERIFY(preview.value().value(QStringLiteral("preview")).toBool());
        QVERIFY(preview.value().value(QStringLiteral("deletedFiles")).toInt() >= 1);

        QVariantMap patch{
            {QStringLiteral("auto_clean"), true},
            {QStringLiteral("max_cache_size_mb"), 2048},
            {QStringLiteral("delete_after_days"), 45}
        };
        QDBusReply<QVariantMap> setPolicy = iface.call(QStringLiteral("SetPolicy"), patch);
        QVERIFY(setPolicy.isValid());
        QVERIFY(setPolicy.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(setPolicy.value().value(QStringLiteral("auto_clean")).toBool(), true);
        QCOMPARE(setPolicy.value().value(QStringLiteral("max_cache_size_mb")).toInt(), 2048);
        QCOMPARE(setPolicy.value().value(QStringLiteral("delete_after_days")).toInt(), 45);

        QDBusReply<QVariantMap> getPolicy = iface.call(QStringLiteral("GetPolicy"));
        QVERIFY(getPolicy.isValid());
        QVERIFY(getPolicy.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(getPolicy.value().value(QStringLiteral("auto_clean")).toBool(), true);

        QVariantMap cleanOptions{
            {QStringLiteral("mode"), QStringLiteral("full")},
            {QStringLiteral("days"), 1},
            {QStringLiteral("clearThumbnail"), true},
            {QStringLiteral("clearFailedThumbnail"), true},
            {QStringLiteral("selectedApps"), QStringList{
                 m_tempDir.path() + QStringLiteral("/cache/google-chrome"),
                 m_tempDir.path() + QStringLiteral("/cache/mesa_shader_cache")
             }}
        };
        QDBusReply<QVariantMap> clean = iface.call(QStringLiteral("Clean"), cleanOptions);
        QVERIFY(clean.isValid());
        QVERIFY(clean.value().value(QStringLiteral("deletedFiles")).toInt() >= 1);

        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }

private:
    QTemporaryDir m_tempDir;
};

QTEST_MAIN(CleanerServiceDbusTest)
#include "cleanerservice_dbus_test.moc"
