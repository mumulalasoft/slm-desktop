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

    void openingCenter_marksReadAndClearsTransientBanners()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        const uint id = manager.NotifyModern(QStringLiteral("org.test.center"),
                                             QStringLiteral("Center"),
                                             QStringLiteral("Body"),
                                             QString(),
                                             {},
                                             QStringLiteral("normal"));
        QVERIFY(id > 0);
        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 1);
        QCOMPARE(manager.unreadCount(), 1);
        QVERIFY(!manager.centerVisible());

        manager.toggleCenter();

        QVERIFY(manager.centerVisible());
        QCOMPARE(manager.unreadCount(), 0);
        QCOMPARE(all->rowCount(), 1);
        QCOMPARE(banners->rowCount(), 0);
        const QModelIndex idx = all->index(0, 0);
        QVERIFY(idx.isValid());
        QCOMPARE(all->data(idx, NotificationListModel::ReadRole).toBool(), true);
    }

    void perAppBannerCap_limitsSpamWithoutDroppingCenterHistory()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        auto *banners = qobject_cast<NotificationListModel *>(manager.bannerNotifications());
        QVERIFY(all);
        QVERIFY(banners);

        for (int i = 0; i < 4; ++i) {
            const uint id = manager.NotifyModern(QStringLiteral("org.test.spam"),
                                                 QStringLiteral("Burst %1").arg(i + 1),
                                                 QStringLiteral("Body"),
                                                 QString(),
                                                 {},
                                                 QStringLiteral("normal"));
            QVERIFY(id > 0);
        }

        QCOMPARE(all->rowCount(), 4);
        QCOMPARE(banners->rowCount(), 3);

        const QModelIndex newestIdx = all->index(0, 0);
        QVERIFY(newestIdx.isValid());
        QCOMPARE(all->data(newestIdx, NotificationListModel::AppIdRole).toString(), QStringLiteral("org.test.spam"));
        QCOMPARE(all->data(newestIdx, NotificationListModel::BannerRole).toBool(), false);
    }

    void perAppSlidingWindowLimiter_suppressesBurstEvenWithLowActiveBanners()
    {
        NotificationManager manager;
        auto *all = qobject_cast<NotificationListModel *>(manager.notifications());
        QVERIFY(all);

        constexpr int burstCount = 7; // limiter default allows 6 attempts / 10s window
        for (int i = 0; i < burstCount; ++i) {
            const uint id = manager.NotifyModern(QStringLiteral("org.test.burst"),
                                                 QStringLiteral("Burst %1").arg(i + 1),
                                                 QStringLiteral("Body"),
                                                 QString(),
                                                 {},
                                                 QStringLiteral("normal"));
            QVERIFY(id > 0);
            // Keep active banner count low so this test validates time-window limiter,
            // not the active-banner cap.
            manager.dismissBanner(id);
        }

        QCOMPARE(all->rowCount(), burstCount);
        int bannerTrueCount = 0;
        for (int row = 0; row < all->rowCount(); ++row) {
            const QModelIndex idx = all->index(row, 0);
            QVERIFY(idx.isValid());
            if (all->data(idx, NotificationListModel::BannerRole).toBool()) {
                ++bannerTrueCount;
            }
        }

        QCOMPARE(bannerTrueCount, 6);
        const QModelIndex newestIdx = all->index(0, 0);
        QVERIFY(newestIdx.isValid());
        QCOMPARE(all->data(newestIdx, NotificationListModel::AppIdRole).toString(), QStringLiteral("org.test.burst"));
        QCOMPARE(all->data(newestIdx, NotificationListModel::BannerRole).toBool(), false);
    }
};

QTEST_MAIN(NotificationManagerPriorityRoutingTest)
#include "notificationmanager_priority_routing_test.moc"
