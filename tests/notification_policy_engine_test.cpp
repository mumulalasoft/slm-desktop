#include <QtTest/QtTest>

#include "src/services/notifications/notificationpolicyengine.h"

class NotificationPolicyEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizePriority_acceptsSupportedValues()
    {
        QCOMPARE(NotificationPolicyEngine::normalizePriority(QStringLiteral("critical")), QStringLiteral("critical"));
        QCOMPARE(NotificationPolicyEngine::normalizePriority(QStringLiteral("HIGH")), QStringLiteral("high"));
        QCOMPARE(NotificationPolicyEngine::normalizePriority(QStringLiteral("normal")), QStringLiteral("normal"));
        QCOMPARE(NotificationPolicyEngine::normalizePriority(QStringLiteral("low")), QStringLiteral("low"));
        QCOMPARE(NotificationPolicyEngine::normalizePriority(QStringLiteral("silent")), QStringLiteral("silent"));
        QCOMPARE(NotificationPolicyEngine::normalizePriority(QStringLiteral("weird")), QStringLiteral("normal"));
    }

    void urgencyForPriority_mapsExpectedLevels()
    {
        QCOMPARE(NotificationPolicyEngine::urgencyForPriority(QStringLiteral("critical")), 2);
        QCOMPARE(NotificationPolicyEngine::urgencyForPriority(QStringLiteral("high")), 2);
        QCOMPARE(NotificationPolicyEngine::urgencyForPriority(QStringLiteral("normal")), 1);
        QCOMPARE(NotificationPolicyEngine::urgencyForPriority(QStringLiteral("low")), 0);
        QCOMPARE(NotificationPolicyEngine::urgencyForPriority(QStringLiteral("silent")), 0);
    }

    void shouldDropNotification_whenDisabled_allowsOnlyCriticalIfPolicyAllows()
    {
        NotificationPolicyEngine::Snapshot snapshot;
        snapshot.notificationsEnabled = false;
        snapshot.allowCriticalAlerts = true;

        NotificationEntry normal;
        normal.priority = QStringLiteral("normal");
        QVERIFY(NotificationPolicyEngine::shouldDropNotification(normal, snapshot));

        NotificationEntry critical;
        critical.priority = QStringLiteral("critical");
        QVERIFY(!NotificationPolicyEngine::shouldDropNotification(critical, snapshot));

        snapshot.allowCriticalAlerts = false;
        QVERIFY(NotificationPolicyEngine::shouldDropNotification(critical, snapshot));
    }

    void shouldShowBanner_appliesContextAndQuietPolicies()
    {
        NotificationPolicyEngine::Snapshot snapshot;
        snapshot.notificationsEnabled = true;
        snapshot.allowCriticalAlerts = true;
        snapshot.silenceFullscreen = true;
        snapshot.silenceScreenShare = true;
        snapshot.focusModeIntegration = true;
        snapshot.deliverQuietly = false;
        snapshot.doNotDisturb = false;

        NotificationRuntimeContext context;
        NotificationEntry normal;
        normal.priority = QStringLiteral("normal");
        QVERIFY(NotificationPolicyEngine::shouldShowBanner(normal, snapshot, context));

        snapshot.doNotDisturb = true;
        QVERIFY(!NotificationPolicyEngine::shouldShowBanner(normal, snapshot, context));
        snapshot.doNotDisturb = false;

        context.fullscreen = true;
        QVERIFY(!NotificationPolicyEngine::shouldShowBanner(normal, snapshot, context));
        context.fullscreen = false;

        context.screenShare = true;
        QVERIFY(!NotificationPolicyEngine::shouldShowBanner(normal, snapshot, context));
        context.screenShare = false;

        context.focusMode = true;
        QVERIFY(!NotificationPolicyEngine::shouldShowBanner(normal, snapshot, context));
        context.focusMode = false;

        snapshot.deliverQuietly = true;
        QVERIFY(!NotificationPolicyEngine::shouldShowBanner(normal, snapshot, context));

        NotificationEntry critical;
        critical.priority = QStringLiteral("critical");
        QVERIFY(NotificationPolicyEngine::shouldShowBanner(critical, snapshot, context));
    }
};

QTEST_GUILESS_MAIN(NotificationPolicyEngineTest)
#include "notification_policy_engine_test.moc"
