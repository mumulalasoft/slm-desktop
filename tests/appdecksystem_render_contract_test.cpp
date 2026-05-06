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

class AppDeckSystemRenderContractTest : public QObject
{
    Q_OBJECT

private slots:
    void appDeckWindow_hasSingleStateOwnerContract()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("state: root.appDeckGridRequested")));
        QVERIFY(text.contains(QStringLiteral("property string mode:")));
        QVERIFY(text.contains(QStringLiteral("function enterDock()")));
        QVERIFY(text.contains(QStringLiteral("function enterGrid()")));
        QVERIFY(text.contains(QStringLiteral("function enterPulse()")));
        QVERIFY(text.contains(QStringLiteral("function hideAppDeck()")));
        QVERIFY(text.contains(QStringLiteral("AppDeckComp.AppDeckCollapsedView")));
        QVERIFY(text.contains(QStringLiteral("AppDeckComp.AppDeckGridAppsView")));
        QVERIFY(text.contains(QStringLiteral("AppDeckComp.AppDeckContextView")));
    }

    void dockRenderer_usesLocalResolvers()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/appdeck/AppDeck.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("property var iconRectsCache")));
        QVERIFY(text.contains(QStringLiteral("function nearestPinnedIndex(globalX)")));
        QVERIFY(text.contains(QStringLiteral("function insertionPinnedPosFromGlobalX(globalX)")));
        QVERIFY(text.contains(QStringLiteral("root.iconRectsCache = rects")));
        QVERIFY(text.contains(QStringLiteral("signal iconRectsChanged")));
    }

    void appDeckWindow_noLongerDependsOnLegacyDockSystem()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString dockWindowPath = base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml");
        const QString dockWindow = readTextFile(dockWindowPath);
        QVERIFY2(!dockWindow.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(dockWindowPath)));

        QVERIFY(!dockWindow.contains(QStringLiteral("AppDeckSystem.")));
        QVERIFY(dockWindow.contains(QStringLiteral("required property var pulseResultsModel")));
        QVERIFY(dockWindow.contains(QStringLiteral("readonly property var dockItem: dockView.dockItem")));
    }

    void dockActive_limitsLayerSurfaceInputRegion()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property int dockInputX")));
        QVERIFY(text.contains(QStringLiteral("readonly property int dockInputY")));
        QVERIFY(text.contains(QStringLiteral("readonly property int dockInputWidth")));
        QVERIFY(text.contains(QStringLiteral("readonly property int dockInputHeight")));
        QVERIFY(text.contains(QStringLiteral("AppDeckLayerShell.setDock(root,")));
        QVERIFY(text.contains(QStringLiteral("AppDeckLayerShell.setGrid(root,")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputX")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputY")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputWidth")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputHeight")));
        QVERIFY(text.contains(QStringLiteral("target: dockView && dockView.dockItem ? dockView.dockItem : null")));
    }

    void appDeckWindow_usesLayerShellInsteadOfWindowStackingHacks()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("AppDeckLayerShell")));
        QVERIFY(!text.contains(QStringLiteral("WindowStaysOnTopHint")));
        QVERIFY(!text.contains(QStringLiteral("FramelessWindowHint")));
        QVERIFY(!text.contains(QStringLiteral("Qt.Tool")));
        QVERIFY(!text.contains(QStringLiteral(".raise(")));
        QVERIFY(!text.contains(QStringLiteral("overlay restack")));
    }

    void crownWindow_usesDedicatedLayerShellSurface()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/overlay/CrownWindow.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("Window {")));
        QVERIFY(text.contains(QStringLiteral("CrownLayerShell")));
        QVERIFY(text.contains(QStringLiteral("prepareTopBarWindow(root)")));
        QVERIFY(text.contains(QStringLiteral("CrownLayerShell.setTopBar(root,")));
        QVERIFY(text.contains(QStringLiteral("transientParent: null")));
        QVERIFY(!text.contains(QStringLiteral("WindowStaysOnTopHint")));
        QVERIFY(!text.contains(QStringLiteral("Qt.Tool")));
        QVERIFY(!text.contains(QStringLiteral(".raise(")));
    }
};

QTEST_MAIN(AppDeckSystemRenderContractTest)
#include "appdecksystem_render_contract_test.moc"
