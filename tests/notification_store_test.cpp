#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "src/services/notifications/notificationstore.h"

class NotificationStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void saveLoad_roundtrip_preservesCoreFields()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const QString path = temp.filePath(QStringLiteral("history.json"));

        QVector<NotificationEntry> entries;
        NotificationEntry first;
        first.id = 10;
        first.appId = QStringLiteral("org.test.first");
        first.appName = QStringLiteral("First App");
        first.summary = QStringLiteral("First Summary");
        first.body = QStringLiteral("First Body");
        first.priority = QStringLiteral("high");
        first.groupId = QStringLiteral("org.test.first");
        first.read = false;
        first.banner = true;
        first.lifecycleState = QStringLiteral("Visible");
        first.actions = {QStringLiteral("default"), QStringLiteral("Open")};
        first.timestamp = QDateTime::currentDateTimeUtc();

        NotificationEntry second;
        second.id = 11;
        second.appId = QStringLiteral("org.test.second");
        second.appName = QStringLiteral("Second App");
        second.summary = QStringLiteral("Second Summary");
        second.body = QStringLiteral("Second Body");
        second.priority = QStringLiteral("low");
        second.groupId = QStringLiteral("org.test.second");
        second.read = true;
        second.banner = false;
        second.lifecycleState = QStringLiteral("Archived");
        second.actions = {QStringLiteral("dismiss")};
        second.timestamp = QDateTime::currentDateTimeUtc();

        entries.push_back(first);
        entries.push_back(second);

        QVERIFY(NotificationStore::saveHistory(path, 42, entries));

        const NotificationStore::LoadedState loaded = NotificationStore::loadHistory(path, 200);
        QVERIFY(loaded.ok);
        QCOMPARE(loaded.nextId, static_cast<uint>(42));
        QCOMPARE(loaded.entries.size(), 2);

        const NotificationEntry l0 = loaded.entries.at(0);
        QCOMPARE(l0.id, static_cast<uint>(10));
        QCOMPARE(l0.appId, QStringLiteral("org.test.first"));
        QCOMPARE(l0.priority, QStringLiteral("high"));
        QCOMPARE(l0.read, false);
        QCOMPARE(l0.lifecycleState, QStringLiteral("Visible"));
        QCOMPARE(l0.actions.size(), 2);

        const NotificationEntry l1 = loaded.entries.at(1);
        QCOMPARE(l1.id, static_cast<uint>(11));
        QCOMPARE(l1.appId, QStringLiteral("org.test.second"));
        QCOMPARE(l1.priority, QStringLiteral("low"));
        QCOMPARE(l1.read, true);
        QCOMPARE(l1.lifecycleState, QStringLiteral("Archived"));
        QCOMPARE(l1.actions.size(), 1);
    }
};

QTEST_GUILESS_MAIN(NotificationStoreTest)
#include "notification_store_test.moc"
