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

class NotificationVisualContractTest : public QObject
{
    Q_OBJECT

private slots:
    void theme_exposesNotificationVisualTokens()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/third_party/slm-style/qml/SlmStyle/Theme.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property real notificationCardRadius: 14")));
        QVERIFY(text.contains(QStringLiteral("readonly property int notificationCardPadding: 14")));
        QVERIFY(text.contains(QStringLiteral("readonly property int notificationCardPaddingMin: 12")));
        QVERIFY(text.contains(QStringLiteral("readonly property int notificationCardPaddingMax: 16")));
        QVERIFY(text.contains(QStringLiteral("readonly property int notificationVerticalRhythm: 8")));
        QVERIFY(text.contains(QStringLiteral("notificationCardBackgroundLight: Qt.rgba(1, 1, 1, 0.7)")));
        QVERIFY(text.contains(QStringLiteral("notificationCardBackgroundDark: Qt.rgba(30 / 255, 30 / 255, 30 / 255, 0.7)")));
    }

    void notificationCard_usesVisualTokens()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/notification/NotificationCard.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("radius: Theme.notificationCardRadius")));
        QVERIFY(text.contains(QStringLiteral("color: Theme.notificationCardBackground")));
        QVERIFY(text.contains(QStringLiteral("anchors.margins: Theme.notificationCardPadding")));
        QVERIFY(text.contains(QStringLiteral("spacing: Theme.notificationVerticalRhythm")));
        QVERIFY(text.contains(QStringLiteral("return Math.min(total, 3)")));
    }

    void notificationCenter_usesVisualTokens()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/notification/NotificationCenter.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("anchors.margins: Theme.notificationCardPadding")));
        QVERIFY(text.contains(QStringLiteral("spacing: Theme.notificationVerticalRhythm")));
    }
};

QTEST_MAIN(NotificationVisualContractTest)
#include "notification_visual_contract_test.moc"
