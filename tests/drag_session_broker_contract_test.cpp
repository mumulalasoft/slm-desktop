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

class DragSessionBrokerContractTest : public QObject
{
    Q_OBJECT

private slots:
    void fileManagerDrag_publishesToShellStateController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                             + QStringLiteral("/Qml/apps/filemanager/FileManagerContentDnd.js");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("\"source_component\": \"filemanager.content\"")));
        QVERIFY(text.contains(QStringLiteral("root.setGlobalDragSession(")));
        QVERIFY(text.contains(QStringLiteral("root.clearGlobalDragSession()")));
    }

    void workspaceDrag_publishesToShellStateController()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                             + QStringLiteral("/Qml/components/workspace/WorkspaceOverlay.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("\"source_component\": \"workspace.overview\"")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setDragSession(")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.clearDragSession(")));
    }
};

QTEST_MAIN(DragSessionBrokerContractTest)
#include "drag_session_broker_contract_test.moc"
