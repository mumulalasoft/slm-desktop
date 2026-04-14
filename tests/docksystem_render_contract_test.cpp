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

class DockSystemRenderContractTest : public QObject
{
    Q_OBJECT

private slots:
    void dockSystem_hasSinglePipelinePhaseContract()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/DockSystem.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property string shellMode")));
        QVERIFY(text.contains(QStringLiteral("launchpadActive")));
        QVERIFY(text.contains(QStringLiteral("readonly property bool dockVisible")));
        QVERIFY(text.contains(QStringLiteral("function resolveNearestPinnedModelIndex")));
    }

    void dockRenderer_usesGlobalResolvers()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/dock/Dock.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("DockSystem.resolveNearestPinnedModelIndex")));
        QVERIFY(text.contains(QStringLiteral("DockSystem.resolveInsertionPinnedPos")));
        QVERIFY(text.contains(QStringLiteral("signal iconRectsChanged")));
    }

    void dockHosts_publishRectsAndUseSharedLayout()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString dockWindowPath = base + QStringLiteral("/Qml/components/overlay/DockWindow.qml");
        const QString dockWindow = readTextFile(dockWindowPath);
        QVERIFY2(!dockWindow.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(dockWindowPath)));

        QVERIFY(dockWindow.contains(QStringLiteral("onIconRectsChanged: function(rects)")));
        QVERIFY(dockWindow.contains(QStringLiteral("DockSystem.updateIconRects(rects)")));
        QVERIFY(dockWindow.contains(QStringLiteral("DockSystem.dockLayoutState")));
        QVERIFY(dockWindow.contains(QStringLiteral("DockSystem.registerDockItem(item)")));
    }
};

QTEST_MAIN(DockSystemRenderContractTest)
#include "docksystem_render_contract_test.moc"
