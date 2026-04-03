#include <QtTest/QtTest>

#include "src/services/notifications/notificationmanager.h"

class NotificationManagerPriorityRoutingTest : public QObject
{
    Q_OBJECT

private slots:
    void lowPriority_routesToCenterOnly()
    {
        NotificationManager manager;
        QCOMPARE(manager.count(), 0);
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);
        QVERIFY(manager.latestNotification().isEmpty());

        const uint id = manager.NotifyModern(QStringLiteral("org.test.low"),
                                             QStringLiteral("Low"),
                                             QStringLiteral("Body"),
                                             QString(),
                                             {},
                                             QStringLiteral("low"));
        QVERIFY(id > 0);

        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 0);
        const QModelIndex idx = all->index(0, 0);
        QVERIFY(idx.isValid());
        QCOMPARE(all->data(idx, NotificationListModel::PriorityRole).toString(), QStringLiteral("low"));
        QCOMPARE(all->data(idx, NotificationListModel::BannerRole).toBool(), false);
        QVERIFY(manager.latestNotification().isEmpty());
    }

    void normalPriority_routesToBannerAndUpdatesLatest()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        const uint id = manager.NotifyModern(QStringLiteral("org.test.normal"),
                                             QStringLiteral("Normal"),
                                             QStringLiteral("Body"),
                                             QString(),
                                             {},
                                             QStringLiteral("normal"));
        QVERIFY(id > 0);

        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 1);
        const QModelIndex idx = all->index(0, 0);
        QVERIFY(idx.isValid());
        QCOMPARE(all->data(idx, NotificationListModel::PriorityRole).toString(), QStringLiteral("normal"));
        QCOMPARE(all->data(idx, NotificationListModel::BannerRole).toBool(), true);
        QCOMPARE(manager.latestNotification().value(QStringLiteral("id")).toUInt(), id);
    }

    void highPriority_withDnd_suppressesBanner()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        manager.setDoNotDisturb(true);
        QVERIFY(manager.doNotDisturb());

        const uint id = manager.NotifyModern(QStringLiteral("org.test.high"),
                                             QStringLiteral("High"),
                                             QStringLiteral("Body"),
                                             QString(),
                                             {},
                                             QStringLiteral("high"));
        QVERIFY(id > 0);

        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 0);
        const QModelIndex idx = all->index(0, 0);
        QVERIFY(idx.isValid());
        QCOMPARE(all->data(idx, NotificationListModel::PriorityRole).toString(), QStringLiteral("high"));
        QCOMPARE(all->data(idx, NotificationListModel::BannerRole).toBool(), false);
        QVERIFY(manager.latestNotification().isEmpty());
    }

    void enablingDnd_clearsActiveBannersButKeepsCenterHistory()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        const uint normalId = manager.NotifyModern(QStringLiteral("org.test.normal"),
                                                   QStringLiteral("Normal"),
                                                   QStringLiteral("Body"),
                                                   QString(),
                                                   {},
                                                   QStringLiteral("normal"));
        QVERIFY(normalId > 0);
        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 1);

        manager.setDoNotDisturb(true);
        QVERIFY(manager.doNotDisturb());
        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 0);
    }

    void invalidPriority_normalizedToNormal()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        const uint id = manager.NotifyModern(QStringLiteral("org.test.invalid"),
                                             QStringLiteral("Unknown"),
                                             QStringLiteral("Body"),
                                             QString(),
                                             {},
                                             QStringLiteral("urgent-ish"));
        QVERIFY(id > 0);

        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 1);
        const QModelIndex idx = all->index(0, 0);
        QVERIFY(idx.isValid());
        QCOMPARE(all->data(idx, NotificationListModel::PriorityRole).toString(), QStringLiteral("normal"));
        QCOMPARE(all->data(idx, NotificationListModel::UrgencyRole).toInt(), 1);
    }

    void freedesktopUrgencyLow_mapsToLowWithoutBanner()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        QVariantMap hints;
        hints.insert(QStringLiteral("urgency"), 0);
        const uint id = manager.Notify(QStringLiteral("LegacyApp"),
                                       0,
                                       QString(),
                                       QStringLiteral("Low urgency"),
                                       QStringLiteral("Body"),
                                       {},
                                       hints,
                                       -1);
        QVERIFY(id > 0);
        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 0);
        const QModelIndex idx = all->index(0, 0);
        QVERIFY(idx.isValid());
        QCOMPARE(all->data(idx, NotificationListModel::PriorityRole).toString(), QStringLiteral("low"));
        QCOMPARE(all->data(idx, NotificationListModel::UrgencyRole).toInt(), 0);
        QCOMPARE(all->data(idx, NotificationListModel::BannerRole).toBool(), false);
        QVERIFY(manager.latestNotification().isEmpty());
    }

    void freedesktopUrgencyHigh_mapsToHighWithBanner()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        QVariantMap hints;
        hints.insert(QStringLiteral("urgency"), 2);
        const uint id = manager.Notify(QStringLiteral("LegacyApp"),
                                       0,
                                       QString(),
                                       QStringLiteral("High urgency"),
                                       QStringLiteral("Body"),
                                       {},
                                       hints,
                                       -1);
        QVERIFY(id > 0);
        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 1);
        const QModelIndex idx = all->index(0, 0);
        QVERIFY(idx.isValid());
        QCOMPARE(all->data(idx, NotificationListModel::PriorityRole).toString(), QStringLiteral("high"));
        QCOMPARE(all->data(idx, NotificationListModel::UrgencyRole).toInt(), 2);
        QCOMPARE(all->data(idx, NotificationListModel::BannerRole).toBool(), true);
        QCOMPARE(manager.latestNotification().value(QStringLiteral("id")).toUInt(), id);
    }
};

QTEST_MAIN(NotificationManagerPriorityRoutingTest)
#include "notificationmanager_priority_routing_test.moc"
