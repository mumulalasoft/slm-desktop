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

class DesktopSceneAppHubSsotContractTest : public QObject
{
    Q_OBJECT

private slots:
    void apphubVisibility_ownedByShellStateController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/DesktopScene.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property bool apphubVisible: ShellStateController")));
        QVERIFY(!text.contains(QStringLiteral("\n    property bool apphubVisible:")));
        QVERIFY(text.contains(QStringLiteral("function setAppHubVisible(visible)")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setAppHubVisible(v)")));
    }
};

QTEST_MAIN(DesktopSceneAppHubSsotContractTest)
#include "desktopscene_apphub_ssot_contract_test.moc"
