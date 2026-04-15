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

class DockControllerSsotContractTest : public QObject
{
    Q_OBJECT

private slots:
    void dockHoverAndExpanded_areSyncedToShellStateController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/DockController.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("function _syncDockStateToShell()")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setDockHoveredItem(")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setDockExpandedItem(")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setDragSession(")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.clearDragSession(")));
        QVERIFY(text.contains(QStringLiteral("onHoveredItemIdChanged: _syncDockStateToShell()")));
        QVERIFY(text.contains(QStringLiteral("onContextItemIdChanged: _syncDockStateToShell()")));
    }
};

QTEST_MAIN(DockControllerSsotContractTest)
#include "dockcontroller_ssot_contract_test.moc"
