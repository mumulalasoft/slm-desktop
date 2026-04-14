#include <QtTest/QtTest>

#include <QFile>

namespace {

QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

} // namespace

class FileManagerStorageNotificationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void storageNotification_producerIsGlobalServiceLayer()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/storage/storageattachnotifier.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("deviceGroupKey")));
        QVERIFY(text.contains(QStringLiteral("org.slm.Desktop.Storage")));
        QVERIFY(text.contains(QStringLiteral("StorageLocationsChanged")));
        QVERIFY(text.contains(QStringLiteral("External Drive connected")));
        QVERIFY(text.contains(QStringLiteral("%1 volume tersedia")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"open\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"Open\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"eject\")")));
        QVERIFY(text.contains(QStringLiteral("QStringLiteral(\"Eject\")")));
    }

    void storageLocationsUpdated_forwardsRowsToAttachHandler()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/apps/filemanager/FileManagerApiConnections.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("function onStorageLocationsUpdated(rows)")));
        QVERIFY(text.contains(QStringLiteral("handleStorageLocationsUpdated(rows || [])")));
    }
};

QTEST_MAIN(FileManagerStorageNotificationContractTest)
#include "filemanager_storage_notification_contract_test.moc"
