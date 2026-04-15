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
}

class NotificationProgressBadgeContractTest : public QObject
{
    Q_OBJECT

private slots:
    void batchOperationIndicator_mustUseGlobalNotificationManagerForProgress()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/applet/BatchOperationIndicator.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("property int progressNotificationId")));
        QVERIFY(text.contains(QStringLiteral("function syncProgressNotification(forceUpdate)")));
        QVERIFY(text.contains(QStringLiteral("function clearProgressNotification()")));
        QVERIFY(text.contains(QStringLiteral("var replacesId = Math.max(0, Number(progressNotificationId || 0))")));
        QVERIFY(text.contains(QStringLiteral("NotificationManager.Notify(")));
        QVERIFY(text.contains(QStringLiteral("replacesId")));
        QVERIFY(text.contains(QStringLiteral("root.syncProgressNotification(false)")));
    }

    void topbarAndCenter_mustReadBadgeStateFromGlobalNotificationManager()
    {
        const QString appletPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/applet/NotificationApplet.qml");
        const QString centerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/notification/NotificationCenter.qml");
        const QString topbarPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/topbar/TopBar.qml");
        const QString dockDelegatePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/dock/DockAppDelegate.qml");
        const QString notifHeaderPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/services/notifications/notificationmanager.h");
        const QString appModelHeaderPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/appmodel.h");
        const QString appModelCppPath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/appmodel.cpp");

        const QString appletText = readTextFile(appletPath);
        const QString centerText = readTextFile(centerPath);
        const QString topbarText = readTextFile(topbarPath);
        const QString dockDelegateText = readTextFile(dockDelegatePath);
        const QString notifHeaderText = readTextFile(notifHeaderPath);
        const QString appModelHeaderText = readTextFile(appModelHeaderPath);
        const QString appModelCppText = readTextFile(appModelCppPath);

        QVERIFY2(!appletText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(appletPath)));
        QVERIFY2(!centerText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(centerPath)));
        QVERIFY2(!topbarText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(topbarPath)));
        QVERIFY2(!dockDelegateText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(dockDelegatePath)));
        QVERIFY2(!notifHeaderText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(notifHeaderPath)));
        QVERIFY2(!appModelHeaderText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(appModelHeaderPath)));
        QVERIFY2(!appModelCppText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(appModelCppPath)));

        QVERIFY(appletText.contains(QStringLiteral("property var notificationManager: NotificationManager")));
        QVERIFY(appletText.contains(QStringLiteral("notificationManager.unreadCount")));
        QVERIFY(centerText.contains(QStringLiteral("model: root.notificationManager ? root.notificationManager.notifications : null")));
        QVERIFY(centerText.contains(QStringLiteral("section.property: \"groupId\"")));
        QVERIFY(centerText.contains(QStringLiteral("unreadCountForGroup")));
        QVERIFY(centerText.contains(QStringLiteral("groupDisplayName")));
        QVERIFY(topbarText.contains(QStringLiteral("name: \"notification\"")));
        QVERIFY(topbarText.contains(QStringLiteral("name: \"batch_ops\"")));
        QVERIFY(dockDelegateText.contains(QStringLiteral("function _dockBadgeFromService()")));
        QVERIFY(dockDelegateText.contains(QStringLiteral("function _dockBadgeFromNotifications()")));
        QVERIFY(dockDelegateText.contains(QStringLiteral("function _canonicalIdentity()")));
        QVERIFY(dockDelegateText.contains(QStringLiteral("AppModel.canonicalAppIdentity")));
        QVERIFY(dockDelegateText.contains(QStringLiteral("NotificationManager.unreadCountForAppId")));
        QVERIFY(!dockDelegateText.contains(QStringLiteral("NotificationManager.unreadCountForApp(")));
        QVERIFY(dockDelegateText.contains(QStringLiteral("Math.max(_dockBadgeFromService(), _dockBadgeFromNotifications())")));
        QVERIFY(notifHeaderText.contains(QStringLiteral("Q_INVOKABLE int unreadCountForAppId")));
        QVERIFY(notifHeaderText.contains(QStringLiteral("Q_INVOKABLE int unreadCountForGroup")));
        QVERIFY(notifHeaderText.contains(QStringLiteral("Q_INVOKABLE QString groupDisplayName")));
        QVERIFY(!notifHeaderText.contains(QStringLiteral("Q_INVOKABLE int unreadCountForApp(")));
        QVERIFY(appModelHeaderText.contains(QStringLiteral("Q_INVOKABLE QString canonicalAppIdentity")));
        QVERIFY(appModelCppText.contains(QStringLiteral("Q_UNUSED(name);")));
        QVERIFY(appModelCppText.contains(QStringLiteral("return QStringLiteral(\"unknown.app\")")));
    }

    void badgeService_mustBeCanonicalOnly_withLegacyMigration()
    {
        const QString badgePath = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/BadgeService.qml");
        const QString text = readTextFile(badgePath);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(badgePath)));

        QVERIFY(text.contains(QStringLiteral("function canonicalKey(appId)")));
        QVERIFY(text.contains(QStringLiteral("function migrateLegacyKeys()")));
        QVERIFY(text.contains(QStringLiteral("Component.onCompleted: migrateLegacyKeys()")));
        QVERIFY(text.contains(QStringLiteral("return Number(_counts[canonicalKey(appId)] || 0)")));
        QVERIFY(text.contains(QStringLiteral("var key = canonicalKey(appId)")));
    }
};

QTEST_MAIN(NotificationProgressBadgeContractTest)
#include "notification_progress_badge_contract_test.moc"
