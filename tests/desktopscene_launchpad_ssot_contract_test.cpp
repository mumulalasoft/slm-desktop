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

class DesktopSceneLaunchpadSsotContractTest : public QObject
{
    Q_OBJECT

private slots:
    void launchpadVisibility_ownedByShellStateController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/DesktopScene.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property bool launchpadVisible: ShellStateController")));
        QVERIFY(!text.contains(QStringLiteral("\n    property bool launchpadVisible:")));
        QVERIFY(text.contains(QStringLiteral("function setLaunchpadVisible(visible)")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setLaunchpadVisible(v)")));
    }
};

QTEST_MAIN(DesktopSceneLaunchpadSsotContractTest)
#include "desktopscene_launchpad_ssot_contract_test.moc"
