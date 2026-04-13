#include <QtTest/QtTest>

#include "src/services/storage/storagepolicystore.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QDir>

using Slm::Storage::StoragePolicyStore;

namespace {

QJsonObject readPolicyRoot(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return {};
    }
    return doc.object();
}

bool writePolicyRoot(const QString &path, const QJsonObject &root)
{
    const QFileInfo fi(path);
    if (!QDir().mkpath(fi.absolutePath())) {
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

class StoragePolicyStoreMigrationTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_prevConfigHome = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", m_tmp.path().toUtf8());
    }

    void cleanup()
    {
        if (m_prevConfigHome.isEmpty()) {
            qunsetenv("XDG_CONFIG_HOME");
        } else {
            qputenv("XDG_CONFIG_HOME", m_prevConfigHome);
        }
    }

    void loadPolicyMap_migratesSchema2AndCanonicalizesKeys()
    {
        const QString path = StoragePolicyStore::policyFilePath();
        QVERIFY(!path.isEmpty());

        QJsonObject entries;
        entries.insert(QStringLiteral("UUID:ABCD-EF01"),
                       QJsonObject{{QStringLiteral("automount"), true},
                                   {QStringLiteral("auto_open"), false},
                                   {QStringLiteral("visible"), true},
                                   {QStringLiteral("read_only"), false},
                                   {QStringLiteral("exec"), false}});
        entries.insert(QStringLiteral("SERIAL-INDEX:USBDISK#2"),
                       QJsonObject{{QStringLiteral("automount"), false},
                                   {QStringLiteral("visible"), true}});

        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 2);
        root.insert(QStringLiteral("entries"), entries);
        QVERIFY(writePolicyRoot(path, root));

        const QVariantMap loaded = StoragePolicyStore::loadPolicyMap();
        QVERIFY(loaded.contains(QStringLiteral("uuid:abcd-ef01")));
        QVERIFY(loaded.contains(QStringLiteral("serial-index:usbdisk#2")));

        const QJsonObject rewritten = readPolicyRoot(path);
        QCOMPARE(rewritten.value(QStringLiteral("schemaVersion")).toInt(), 3);
        const QJsonObject rewrittenEntries = rewritten.value(QStringLiteral("entries")).toObject();
        QVERIFY(rewrittenEntries.contains(QStringLiteral("uuid:abcd-ef01")));
        QVERIFY(rewrittenEntries.contains(QStringLiteral("serial-index:usbdisk#2")));
    }

    void savePolicyMap_writesCanonicalKeys()
    {
        QVariantMap policyMap;
        policyMap.insert(QStringLiteral("PARTUUID:ABC-123"),
                         QVariantMap{{QStringLiteral("automount"), true},
                                     {QStringLiteral("action"), QStringLiteral("mount")},
                                     {QStringLiteral("visible"), true}});

        QString err;
        QVERIFY(StoragePolicyStore::savePolicyMap(policyMap, &err));
        QVERIFY2(err.isEmpty(), qPrintable(err));

        const QString path = StoragePolicyStore::policyFilePath();
        const QJsonObject root = readPolicyRoot(path);
        QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 3);
        const QJsonObject entries = root.value(QStringLiteral("entries")).toObject();
        QVERIFY(entries.contains(QStringLiteral("partuuid:abc-123")));
        QVERIFY(!entries.contains(QStringLiteral("PARTUUID:ABC-123")));
        const QJsonObject row = entries.value(QStringLiteral("partuuid:abc-123")).toObject();
        QCOMPARE(row.value(QStringLiteral("action")).toString(), QStringLiteral("mount"));
    }

    void saveLoad_roundtripPersistsPolicyData()
    {
        QVariantMap initial;
        initial.insert(QStringLiteral("uuid:disk-a"),
                       QVariantMap{{QStringLiteral("action"), QStringLiteral("mount")},
                                   {QStringLiteral("automount"), true},
                                   {QStringLiteral("auto_open"), true},
                                   {QStringLiteral("visible"), true},
                                   {QStringLiteral("read_only"), false},
                                   {QStringLiteral("exec"), false}});
        initial.insert(QStringLiteral("serial-index:usb123#2"),
                       QVariantMap{{QStringLiteral("action"), QStringLiteral("ask")},
                                   {QStringLiteral("automount"), false},
                                   {QStringLiteral("auto_open"), false},
                                   {QStringLiteral("visible"), true},
                                   {QStringLiteral("read_only"), true},
                                   {QStringLiteral("exec"), false}});

        QString err;
        QVERIFY(StoragePolicyStore::savePolicyMap(initial, &err));
        QVERIFY2(err.isEmpty(), qPrintable(err));

        const QVariantMap loadedOnce = StoragePolicyStore::loadPolicyMap();
        QVERIFY(loadedOnce.contains(QStringLiteral("uuid:disk-a")));
        QVERIFY(loadedOnce.contains(QStringLiteral("serial-index:usb123#2")));
        QCOMPARE(loadedOnce.value(QStringLiteral("uuid:disk-a")).toMap().value(QStringLiteral("auto_open")).toBool(),
                 true);
        QCOMPARE(loadedOnce.value(QStringLiteral("serial-index:usb123#2"))
                         .toMap()
                         .value(QStringLiteral("read_only"))
                         .toBool(),
                 true);

        // Simulate "restart" by saving loaded map and loading again.
        QVERIFY(StoragePolicyStore::savePolicyMap(loadedOnce, &err));
        QVERIFY2(err.isEmpty(), qPrintable(err));
        const QVariantMap loadedTwice = StoragePolicyStore::loadPolicyMap();
        QCOMPARE(loadedOnce, loadedTwice);
    }

    void loadPolicyMap_corruptFileCreatesBackupAndReturnsEmpty()
    {
        const QString path = StoragePolicyStore::policyFilePath();
        QVERIFY(!path.isEmpty());

        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("{ this-is-invalid-json ");
        f.close();

        const QString backupPath = path + QStringLiteral(".corrupt");
        QFile::remove(backupPath);

        const QVariantMap loaded = StoragePolicyStore::loadPolicyMap();
        QVERIFY(loaded.isEmpty());
        QVERIFY2(QFileInfo::exists(backupPath), qPrintable(QStringLiteral("missing backup: %1").arg(backupPath)));
    }

private:
    QTemporaryDir m_tmp;
    QByteArray m_prevConfigHome;
};

QTEST_MAIN(StoragePolicyStoreMigrationTest)
#include "storagepolicystore_migration_test.moc"
