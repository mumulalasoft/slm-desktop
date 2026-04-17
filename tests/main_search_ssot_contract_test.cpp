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

class MainSearchSsotContractTest : public QObject
{
    Q_OBJECT

private slots:
    void searchVisibility_ownedByShellStateController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Main.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property bool searchVisible: (typeof ShellStateController")));
        QVERIFY(text.contains(QStringLiteral("readonly property bool tothespotVisible: searchVisible")));
        QVERIFY(text.contains(QStringLiteral("function setSearchVisible(visible)")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setToTheSpotVisible(v)")));
        QVERIFY(!text.contains(QStringLiteral("\n    property bool tothespotVisible:")));
    }

    void searchQuery_ownedByShellStateController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Main.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property string searchQuery: (typeof ShellStateController")));
        QVERIFY(text.contains(QStringLiteral("readonly property string tothespotQuery: searchQuery")));
        QVERIFY(text.contains(QStringLiteral("function setSearchQuery(query)")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setSearchQuery(text)")));
        QVERIFY(!text.contains(QStringLiteral("\n    property string tothespotQuery:")));
    }
};

QTEST_MAIN(MainSearchSsotContractTest)
#include "main_search_ssot_contract_test.moc"
