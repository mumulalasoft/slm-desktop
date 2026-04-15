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

class NotificationInputRegionContractTest : public QObject
{
    Q_OBJECT

private slots:
    void notificationWindowMustUseCompactSurfaceForBanners()
    {
        const QString windowPath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/notification/NotificationManagerWindow.qml");
        const QString managerPath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/notification/NotificationManager.qml");

        const QString windowText = readTextFile(windowPath);
        const QString managerText = readTextFile(managerPath);

        QVERIFY2(!windowText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(windowPath)));
        QVERIFY2(!managerText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(managerPath)));

        QVERIFY(windowText.contains(QStringLiteral("readonly property bool centerOpen")));
        QVERIFY(windowText.contains(QStringLiteral("readonly property bool bannerVisible")));
        QVERIFY(windowText.contains(QStringLiteral("visible: !!parentWindow && parentWindow.visible && (centerOpen || bannerVisible)")));
        QVERIFY(windowText.contains(QStringLiteral("if (!centerOpen && notificationLayer)")));
        QVERIFY(windowText.contains(QStringLiteral("notificationLayer.bannerSurfaceWidth")));
        QVERIFY(windowText.contains(QStringLiteral("notificationLayer.bannerSurfaceHeight")));

        QVERIFY(managerText.contains(QStringLiteral("readonly property int bannerSurfaceWidth")));
        QVERIFY(managerText.contains(QStringLiteral("readonly property int bannerSurfaceHeight")));
        QVERIFY(managerText.contains(QStringLiteral("readonly property bool bannerVisible")));
    }
};

QTEST_MAIN(NotificationInputRegionContractTest)
#include "notification_input_region_contract_test.moc"
