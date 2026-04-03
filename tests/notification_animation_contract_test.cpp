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

class NotificationAnimationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void theme_exposesNotificationAnimationTokens()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/third_party/slm-style/qml/SlmStyle/Theme.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("notificationBannerEnterDuration")));
        QVERIFY(text.contains(QStringLiteral("notificationBannerExitDuration")));
        QVERIFY(text.contains(QStringLiteral("notificationCenterSlideDuration")));
        QVERIFY(text.contains(QStringLiteral("notificationCenterDimDuration")));
        QVERIFY(text.contains(QStringLiteral("notificationBannerEntryOffset")));
        QVERIFY(text.contains(QStringLiteral("notificationBannerExitScale")));
    }

    void bannerContainer_usesThemeAnimationContract()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/notification/BannerContainer.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("duration: Theme.notificationBannerEnterDuration")));
        QVERIFY(text.contains(QStringLiteral("duration: Theme.notificationBannerExitDuration")));
        QVERIFY(text.contains(QStringLiteral("from: Theme.notificationBannerEntryOffset")));
        QVERIFY(text.contains(QStringLiteral("to: Theme.notificationBannerExitScale")));
        QVERIFY(text.contains(QStringLiteral("interval: root.microAnimationAllowed() ? Theme.notificationBannerExitDuration : 1")));
    }

    void center_usesThemeAnimationContract()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/notification/NotificationCenter.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("duration: Theme.notificationCenterDimDuration")));
        QVERIFY(text.contains(QStringLiteral("duration: Theme.notificationCenterSlideDuration")));
        QVERIFY(text.contains(QStringLiteral("easing.type: Theme.easingDecelerate")));
    }

    void manager_wiresBannerAutoDismissToBackendDuration()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/notification/NotificationManager.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("autoDismissMs: {")));
        QVERIFY(text.contains(QStringLiteral("root.notificationManager.bubbleDurationMs")));
        QVERIFY(text.contains(QStringLiteral("return v > 0 ? v : 6200")));
    }
};

QTEST_MAIN(NotificationAnimationContractTest)
#include "notification_animation_contract_test.moc"
