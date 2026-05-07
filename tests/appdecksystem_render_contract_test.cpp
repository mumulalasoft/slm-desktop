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

    void appDeckWindow_usesPersistentTopLayerShellSurface()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("AppDeckLayerShell")));
        QVERIFY(text.contains(QStringLiteral("Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint")));
        QVERIFY(text.contains(QStringLiteral("transientParent: null")));
        QVERIFY(text.contains(QStringLiteral("&& root.layerShellSupported")));
        QVERIFY(text.contains(QStringLiteral("height: appDeckHidden")));
        QVERIFY(!text.contains(QStringLiteral("WindowStaysOnTopHint")));
        QVERIFY(!text.contains(QStringLiteral("Qt.Tool")));
        QVERIFY(!text.contains(QStringLiteral(".raise(")));
        QVERIFY(!text.contains(QStringLiteral("requestActivate(")));
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
        QVERIFY(text.contains(QStringLiteral("readonly property int safePanelHeight")));
        QVERIFY(text.contains(QStringLiteral("height: root.policySurfaceHeight")));
        QVERIFY(text.contains(QStringLiteral("readonly property bool policyContentVisible")));
        QVERIFY(text.contains(QStringLiteral("!rootWindow.lockScreenVisible")));
        QVERIFY(text.contains(QStringLiteral("&& root.layerShellSupported")));
        QVERIFY(text.contains(QStringLiteral("Math.max(0, Math.round(root.effectiveContentVisible")));
        QVERIFY(text.contains(QStringLiteral("transientParent: null")));
        QVERIFY(!text.contains(QStringLiteral("WindowStaysOnTopHint")));
        QVERIFY(!text.contains(QStringLiteral("Qt.Tool")));
        QVERIFY(!text.contains(QStringLiteral(".raise(")));
        QVERIFY(!text.contains(QStringLiteral("requestActivate(")));
    }

    void lockScreenWindow_usesSecurityLayerShellOverlay()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString lockPath = base + QStringLiteral("/Qml/components/overlay/LockScreenWindow.qml");
        const QString lockText = readTextFile(lockPath);
        QVERIFY2(!lockText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(lockPath)));

        QVERIFY(lockText.contains(QStringLiteral("SecurityLayerShell")));
        QVERIFY(lockText.contains(QStringLiteral("prepareSecurityOverlayWindow(root)")));
        QVERIFY(lockText.contains(QStringLiteral("SecurityLayerShell.setSecurityOverlay(root,")));
        QVERIFY(lockText.contains(QStringLiteral("transientParent: null")));
        QVERIFY(lockText.contains(QStringLiteral("&& root.layerShellSupported")));
        QVERIFY(!lockText.contains(QStringLiteral("WindowStaysOnTopHint")));
        QVERIFY(!lockText.contains(QStringLiteral("requestActivate(")));
        QVERIFY(lockText.contains(QStringLiteral("title: \"SLM Security Overlay\"")));

        const QString controllerPath = base + QStringLiteral("/src/core/wayland/layershell/appdecklayershellcontroller.cpp");
        const QString controllerText = readTextFile(controllerPath);
        QVERIFY2(!controllerText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(controllerPath)));
        QVERIFY(controllerText.contains(QStringLiteral("prepareSecurityOverlayWindow")));
        QVERIFY(controllerText.contains(QStringLiteral("setSecurityOverlay")));
        QVERIFY(controllerText.contains(QStringLiteral("setScope(QStringLiteral(\"slm-lockscreen\"))")));
        QVERIFY(controllerText.contains(QStringLiteral("LayerShellQt::Window::LayerOverlay")));
        QVERIFY(controllerText.contains(QStringLiteral("KeyboardInteractivityExclusive")));
        QVERIFY(controllerText.contains(QStringLiteral("WlrLayerShell::LayerOverlay")));
        QVERIFY(controllerText.contains(QStringLiteral("WlrLayerShell::KeyboardInteractivityExclusive")));

        const QString mainPath = base + QStringLiteral("/main.cpp");
        const QString mainText = readTextFile(mainPath);
        QVERIFY2(!mainText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(mainPath)));
        QVERIFY(mainText.contains(QStringLiteral("AppDeckLayerShellController securityLayerShell(&wlrLayerShell);")));
        QVERIFY(mainText.contains(QStringLiteral("\"SecurityLayerShell\"")));
    }

    void mainWindow_usesLayerShellOnlySystemSurfaces()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString mainPath = base + QStringLiteral("/Main.qml");
        const QString mainText = readTextFile(mainPath);
        QVERIFY2(!mainText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(mainPath)));

        QVERIFY(mainText.contains(QStringLiteral("readonly property bool crownLayerShellSupported")));
        QVERIFY(!mainText.contains(QStringLiteral("appDeckTopLevelWindowAllowed")));
        QVERIFY(!mainText.contains(QStringLiteral("crownTopLevelWindowAllowed")));
        QVERIFY(mainText.contains(QStringLiteral("sourceComponent: crownLayerShellComponent")));
        QVERIFY(mainText.contains(QStringLiteral("function markStartupTopbarBootstrapReady()")));
        QVERIFY(mainText.contains(QStringLiteral("function maybeAttachAppDeck()")));
        QVERIFY(mainText.contains(QStringLiteral("|| !externalAppDeckWindowAllowed")));
        QVERIFY(mainText.contains(QStringLiteral("function onSurfaceReady()")));
        QVERIFY(!mainText.contains(QStringLiteral("startupTopbarBootstrapTimer")));
        QVERIFY(!mainText.contains(QStringLiteral("appDeckAttachTimer")));
        QVERIFY(mainText.contains(QStringLiteral("OverlayComp.CrownWindow")));
        QVERIFY(!mainText.contains(QStringLiteral("sourceComponent: root.crownLayerShellSupported ? crownLayerShellComponent : crownInlineComponent")));
        QVERIFY(!mainText.contains(QStringLiteral("OverlayComp.CrownInlineLayer")));
        QVERIFY(mainText.contains(QStringLiteral("function _finishUnlockSuccess(lockScreenWindow)")));
        QVERIFY(mainText.contains(QStringLiteral("function _canUseLocalUnlockFallback(password, errorCode)")));
        QVERIFY(mainText.contains(QStringLiteral("code === \"service-unavailable\" || code === \"dbus-call-failed\"")));
        QVERIFY(mainText.contains(QStringLiteral("root._finishUnlockSuccess(lockScreenWindow)")));
        QVERIFY(mainText.contains(QStringLiteral("unlock backend unavailable; using local unlock fallback")));

        QVERIFY(!mainText.contains(QStringLiteral("embeddedAppDeckEnabled: !root.appDeckDisabled")));

        const QString cmakePath = base + QStringLiteral("/CMakeLists.txt");
        const QString cmakeText = readTextFile(cmakePath);
        QVERIFY2(!cmakeText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(cmakePath)));
        QVERIFY(!cmakeText.contains(QStringLiteral("Qml/components/overlay/CrownInlineLayer.qml")));
        QVERIFY(!cmakeText.contains(QStringLiteral("Qml/components/appdeck/AppDeckEmbeddedSurface.qml")));
    }

    void crownTopbarControlsUseStableHeights()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString crownPath = base + QStringLiteral("/Qml/components/crown/Crown.qml");
        const QString crownText = readTextFile(crownPath);
        QVERIFY2(!crownText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(crownPath)));

        QVERIFY(crownText.contains(QStringLiteral("id: leftCluster")));
        QVERIFY(crownText.contains(QStringLiteral("height: Math.max(root.iconButtonH")));
        QVERIFY(crownText.contains(QStringLiteral("width: item ? item.implicitWidth : root.iconButtonW")));
        QVERIFY(crownText.contains(QStringLiteral("menuBarHeight: leftCluster.height")));

        const QString brandPath = base + QStringLiteral("/Qml/components/crown/CrownBrandSection.qml");
        const QString brandText = readTextFile(brandPath);
        QVERIFY2(!brandText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(brandPath)));

        QVERIFY(brandText.contains(QStringLiteral("property int menuBarHeight")));
        QVERIFY(brandText.contains(QStringLiteral("height: Math.max(1, menuBarHeight)")));
        QVERIFY(brandText.contains(QStringLiteral("height: root.menuBarHeight")));
        QVERIFY(brandText.contains(QStringLiteral("function _loadingMenuItems()")));
        QVERIFY(brandText.contains(QStringLiteral("function _emptyMenuItems(menuRow)")));
        QVERIFY(brandText.contains(QStringLiteral("openRetryAttempts >= 25")));
        QVERIFY(brandText.contains(QStringLiteral("dropdown.menuItems = root._emptyMenuItems(modelData)")));
        QVERIFY(brandText.contains(QStringLiteral("function onChanged()")));
    }

    void crownMainMenuUsesThemeMenuAndCenteredLogoSlot()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString anchoredPath = base + QStringLiteral("/Qml/components/crown/CrownAnchoredMenu.qml");
        const QString anchoredText = readTextFile(anchoredPath);
        QVERIFY2(!anchoredText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(anchoredPath)));

        QVERIFY(anchoredText.contains(QStringLiteral("import SlmStyle as DSStyle")));
        QVERIFY(anchoredText.contains(QStringLiteral("DSStyle.Menu {")));

        const QString mainMenuPath = base + QStringLiteral("/Qml/components/crown/CrownMainMenuControl.qml");
        const QString mainMenuText = readTextFile(mainMenuPath);
        QVERIFY2(!mainMenuText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(mainMenuPath)));

        QVERIFY(mainMenuText.contains(QStringLiteral("id: logoSlot")));
        QVERIFY(mainMenuText.contains(QStringLiteral("property int logoVisualOffsetX")));
        QVERIFY(mainMenuText.contains(QStringLiteral("anchors.horizontalCenterOffset: root.logoVisualOffsetX")));
        QVERIFY(mainMenuText.contains(QStringLiteral("anchors.centerIn: parent")));
        QVERIFY(mainMenuText.contains(QStringLiteral("anchors.fill: parent")));
        QVERIFY(mainMenuText.contains(QStringLiteral("DSStyle.MenuItem")));
        QVERIFY(mainMenuText.contains(QStringLiteral("DSStyle.MenuSeparator")));
        QVERIFY(mainMenuText.contains(QStringLiteral("DSStyle.Menu {")));
        QVERIFY(mainMenuText.contains(QStringLiteral("property var rootWindow: null")));
        QVERIFY(mainMenuText.contains(QStringLiteral("function _hasPowerAction(actionName)")));
        QVERIFY(mainMenuText.contains(QStringLiteral("function _hasLockAction()")));
        QVERIFY(mainMenuText.contains(QStringLiteral("function _logOut()")));
        QVERIFY(mainMenuText.contains(QStringLiteral("SessionStateClient.lock()")));
        QVERIFY(mainMenuText.contains(QStringLiteral("root.rootWindow.lockScreenVisible = true")));
        QVERIFY(mainMenuText.contains(QStringLiteral("ShellStateController.setLockScreenActive(true)")));
        QVERIFY(mainMenuText.contains(QStringLiteral("enabled: root._hasLockAction()")));
        QVERIFY(mainMenuText.contains(QStringLiteral("enabled: root._hasPowerAction(\"logout\")")));
        QVERIFY(mainMenuText.contains(QStringLiteral("PowerBridge.logout()")));
        QVERIFY(mainMenuText.contains(QStringLiteral("PowerBridge.reboot()")));
        QVERIFY(mainMenuText.contains(QStringLiteral("PowerBridge.powerOff()")));
        QVERIFY(mainMenuText.contains(QStringLiteral("enabled: root._hasPowerAction(\"reboot\")")));
        QVERIFY(mainMenuText.contains(QStringLiteral("enabled: root._hasPowerAction(\"powerOff\")")));
        QVERIFY(!mainMenuText.contains(QStringLiteral("enabled: typeof PowerBridge !== \"undefined\" && PowerBridge && PowerBridge.canReboot")));
        QVERIFY(!mainMenuText.contains(QStringLiteral("enabled: typeof PowerBridge !== \"undefined\" && PowerBridge && PowerBridge.canPowerOff")));
        QVERIFY(!mainMenuText.contains(QStringLiteral("PowerBridge.shutdown()")));
        QVERIFY(!mainMenuText.contains(QStringLiteral("        MenuItem {")));
        QVERIFY(!mainMenuText.contains(QStringLiteral("        MenuSeparator {")));
        QVERIFY(!mainMenuText.contains(QStringLiteral("        Menu {")));
        QVERIFY(!mainMenuText.contains(QStringLiteral("background: DSStyle.PopupSurface")));

        const QString crownPath = base + QStringLiteral("/Qml/components/crown/Crown.qml");
        const QString crownText = readTextFile(crownPath);
        QVERIFY2(!crownText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(crownPath)));
        QVERIFY(crownText.contains(QStringLiteral("property var rootWindow: null")));
        QVERIFY(crownText.contains(QStringLiteral("rootWindow: root.rootWindow")));

        const QString crownWindowPath = base + QStringLiteral("/Qml/components/overlay/CrownWindow.qml");
        const QString crownWindowText = readTextFile(crownWindowPath);
        QVERIFY2(!crownWindowText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(crownWindowPath)));
        QVERIFY(crownWindowText.contains(QStringLiteral("rootWindow: root.rootWindow")));
    }

    void qemuSmokeWrapper_keepsLayerShellEnabled()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/scripts/dev/qemu-smoke.sh");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("/usr/local/bin/slm-shell.real")));
        QVERIFY(text.contains(QStringLiteral("SLM_FAST_FIRST_FRAME=1")));
        QVERIFY(!text.contains(QStringLiteral("SLM_DISABLE_LAYER_SHELL=1")));
        QVERIFY(!text.contains(QStringLiteral("SLM_DISABLE_APPDECK=1")));
    }
};

QTEST_MAIN(AppDeckSystemRenderContractTest)
#include "appdecksystem_render_contract_test.moc"
