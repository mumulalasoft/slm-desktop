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
        QVERIFY(text.contains(QStringLiteral("function collapseToDock(reason)")));
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
        QVERIFY(text.contains(QStringLiteral("AppDeckLayerShell.setDockAt(root, w, h,")));
        QVERIFY(text.contains(QStringLiteral("AppDeckLayerShell.setGrid(root,")));
        QVERIFY(text.contains(QStringLiteral("root._eagerDockMask = false")));
        QVERIFY(text.contains(QStringLiteral("property bool _forceGridInputRegion: false")));
        QVERIFY(text.contains(QStringLiteral("property bool _gridCollapseGuardsArmed: false")));
        QVERIFY(text.contains(QStringLiteral("property bool _gridPointerDismissArmed: false")));
        QVERIFY(text.contains(QStringLiteral("property bool _gridHadActiveFocus: false")));
        QVERIFY(text.contains(QStringLiteral("function shouldCollapseForActivation()")));
        QVERIFY(text.contains(QStringLiteral("function canDismissGridByPointer()")));
        QVERIFY(text.contains(QStringLiteral("function dismissGridByPointer(reason)")));
        QVERIFY(text.contains(QStringLiteral("onPointerDismissRequested:")));
        QVERIFY(text.contains(QStringLiteral("root.dismissGridByPointer(\"grid-pointer-dismiss\")")));
        QVERIFY(text.contains(QStringLiteral("id: gridCollapseGuardTimer")));
        // P0 (appdeck interaction responsiveness fix) — the legacy
        // `gridPointerDismissTimer` (max(motionSurfaceDuration+1200, 1800) ms)
        // was the visible cause of the "responsive only on fast click" symptom
        // and is replaced by a one-frame Qt.callLater arm inside enterGrid /
        // enterPulse. The arm flag `_gridPointerDismissArmed` is kept.
        QVERIFY(!text.contains(QStringLiteral("id: gridPointerDismissTimer")));
        QVERIFY(text.contains(QStringLiteral("function _armPointerDismissOnNextTick()")));
        QVERIFY(text.contains(QStringLiteral("root._armPointerDismissOnNextTick()")));
        QVERIFY(text.contains(QStringLiteral("[APPDECK] ignore pointer-dismiss reason=")));
        QVERIFY(text.contains(QStringLiteral("[APPDECK] collapseToDock reason=")));
        QVERIFY(text.contains(QStringLiteral("root._forceGridInputRegion = true")));
        QVERIFY(text.contains(QStringLiteral("!root._forceGridInputRegion && (root.state === \"dock\" || root._eagerDockMask)")));
        QVERIFY(text.contains(QStringLiteral("AppDeckLayerShell.setGrid(root, w, h, 0, 0, w, h)")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputX")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputY")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputWidth")));
        QVERIFY(text.contains(QStringLiteral("root.dockInputHeight")));
        QVERIFY(text.contains(QStringLiteral("target: dockView && dockView.dockItem ? dockView.dockItem : null")));
        QVERIFY(text.contains(QStringLiteral("readonly property bool compactDockSurface")));
        QVERIFY(text.contains(QStringLiteral("? root.dockSurfaceWidth")));
        QVERIFY(text.contains(QStringLiteral("dockMarginLeft = Math.max(0, Math.round((screenW - w) * 0.5))")));
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
        QVERIFY(text.contains(QStringLiteral("target: (typeof WindowingBackend !== \"undefined\") ? WindowingBackend : null")));
        QVERIFY(text.contains(QStringLiteral("eventName === \"window-focused\"")));
        QVERIFY(text.contains(QStringLiteral("eventName === \"window-created\"")));
        QVERIFY(text.contains(QStringLiteral("function onFocusedAppIdChanged()")));
        QVERIFY(text.contains(QStringLiteral("root.shouldCollapseForActivation()")));
        QVERIFY(text.contains(QStringLiteral("if (!root.active && root._gridHadActiveFocus && root.shouldCollapseForActivation())")));
        QVERIFY(text.contains(QStringLiteral("root.canDismissGridByPointer()")));
        QVERIFY(text.contains(QStringLiteral("root.collapseToDock(\"window-event\")")));
        QVERIFY(text.contains(QStringLiteral("root.collapseToDock(\"focused-app\")")));
        QVERIFY(text.contains(QStringLiteral("root.collapseToDock(\"dock-app-activated\")")));
        QVERIFY(text.contains(QStringLiteral("root.dismissGridByPointer(\"outside-click\")")));

        const QString layerPath = QStringLiteral(DESKTOP_SOURCE_DIR)
            + QStringLiteral("/src/core/wayland/layershell/appdecklayershellcontroller.cpp");
        const QString layerText = readTextFile(layerPath);
        QVERIFY2(!layerText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(layerPath)));
        QVERIFY(layerText.contains(QStringLiteral("LayerShellQt::Window::AnchorBottom\n        | LayerShellQt::Window::AnchorLeft")));
        QVERIFY(!layerText.contains(QStringLiteral("prepared layer=TopLayer anchors=bottom|left|right exclusiveZone=0 scope=slm-appdeck")));
        QVERIFY(layerText.contains(QStringLiteral("bool AppDeckLayerShellController::setDockAt")));
        QVERIFY(layerText.contains(QStringLiteral("QMargins(qMax(0, marginLeft), 0, 0, qMax(0, marginBottom))")));
        QVERIFY(layerText.contains(QStringLiteral("state.layerWindow->setDesiredSize(QSize(1, 1))")));
        QVERIFY(layerText.contains(QStringLiteral("state.layerWindow->setMargins(layerMargins)")));
    }

    void desktopScene_doesNotLoopInternalStateThroughShowAppGrid()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/DesktopScene.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("function pushAppDeckToCompositor()")));
        QVERIFY(text.contains(QStringLiteral("WindowingBackend.sendCommand(\"appdeck \"")));
        QVERIFY(!text.contains(QStringLiteral("WorkspaceManager.ShowAppGrid()")));
    }

    void mainWindow_wiresAppDeckCollapseRequest()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Main.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("OverlayComp.AppDeckWindow")));
        QVERIFY(text.contains(QStringLiteral("onRequestCollapse:")));
        QVERIFY(text.contains(QStringLiteral("root.setSearchVisible(false)")));
        QVERIFY(text.contains(QStringLiteral("ShellStateController.setAppDeckVisible(false)")));
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
        QVERIFY(lockText.contains(QStringLiteral("property var targetScreen")));
        QVERIFY(lockText.contains(QStringLiteral("[LOCKSCREEN] [MONITOR]")));
        QVERIFY(lockText.contains(QStringLiteral("[LOCKSCREEN] [INPUT_BLOCK]")));
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
        QVERIFY(controllerText.contains(QStringLiteral("SurfaceState &")));

        const QString controllerHeaderPath = base + QStringLiteral("/src/core/wayland/layershell/appdecklayershellcontroller.h");
        const QString controllerHeaderText = readTextFile(controllerHeaderPath);
        QVERIFY2(!controllerHeaderText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(controllerHeaderPath)));
        QVERIFY(controllerHeaderText.contains(QStringLiteral("QHash<QWindow *, SurfaceState>")));

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
        QVERIFY(mainText.contains(QStringLiteral("root._finishUnlockSuccess(lockScreenWindow)")));
        QVERIFY(!mainText.contains(QStringLiteral("_canUseLocalUnlockFallback")));
        QVERIFY(!mainText.contains(QStringLiteral("unlock backend unavailable; using local unlock fallback")));
        QVERIFY(!mainText.contains(QStringLiteral("SessionStateClient.setLocked(false)")));
        QVERIFY(!mainText.contains(QStringLiteral("SessionStateClient.setLocked")));
        QVERIFY(mainText.contains(QStringLiteral("Instantiator")));
        QVERIFY(mainText.contains(QStringLiteral("Qt.application.screens")));
        QVERIFY(mainText.contains(QStringLiteral("targetScreen: modelData")));

        const QString sessionClientPath = base + QStringLiteral("/src/services/session/SessionStateClient.h");
        const QString sessionClientText = readTextFile(sessionClientPath);
        QVERIFY2(!sessionClientText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sessionClientPath)));
        QVERIFY(!sessionClientText.contains(QStringLiteral("Q_INVOKABLE void setLocked")));
        QVERIFY(sessionClientText.contains(QStringLiteral("Q_PROPERTY(QString lockState READ lockState NOTIFY lockStateChanged)")));

        QVERIFY(!mainText.contains(QStringLiteral("embeddedAppDeckEnabled: !root.appDeckDisabled")));

        const QString cmakePath = base + QStringLiteral("/CMakeLists.txt");
        const QString cmakeText = readTextFile(cmakePath);
        QVERIFY2(!cmakeText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(cmakePath)));
        QVERIFY(!cmakeText.contains(QStringLiteral("Qml/components/overlay/CrownInlineLayer.qml")));
        QVERIFY(!cmakeText.contains(QStringLiteral("Qml/components/appdeck/AppDeckEmbeddedSurface.qml")));
        QVERIFY(!QFile::exists(base + QStringLiteral("/Qml/components/appdeck/AppDeckEmbeddedSurface.qml")));
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

    void appDeck_v2_skeletonExists()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        QVERIFY(QFile::exists(base + "/Qml/components/appdeck/v2/AppDeckRoot.qml"));
        QVERIFY(QFile::exists(base + "/Qml/components/appdeck/v2/AppDeckController.qml"));
        QVERIFY(QFile::exists(base + "/Qml/components/appdeck/v2/AppDeckTokens.qml"));

        const QString r = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckRoot.qml");
        QVERIFY2(!r.isEmpty(), "AppDeckRoot.qml unreadable");
        QVERIFY(r.contains(QStringLiteral("property string deckState")));
        QVERIFY(r.contains(QStringLiteral("property string deckMode")));
        QVERIFY(r.contains(QStringLiteral("property string appearanceMode")));
        QVERIFY(r.contains(QStringLiteral("property real morphProgress")));
        QVERIFY(r.contains(QStringLiteral("property bool transitioning")));
        QVERIFY(r.contains(QStringLiteral("property point gravityCenter")));
        QVERIFY(r.contains(QStringLiteral("id: morphAnim")));
        QVERIFY(r.contains(QStringLiteral("easing.type: Theme.easingDecelerate")));

        const QString c = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckController.qml");
        QVERIFY2(!c.isEmpty(), "AppDeckController.qml unreadable");
        QVERIFY(c.contains(QStringLiteral("derivedDeckState")));
        QVERIFY(c.contains(QStringLiteral("derivedDeckMode")));

        const QString t = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckTokens.qml");
        QVERIFY2(!t.isEmpty(), "AppDeckTokens.qml unreadable");
        QVERIFY(t.contains(QStringLiteral("morphDuration")));
        QVERIFY(t.contains(QStringLiteral("dockRadius")));
        QVERIFY(t.contains(QStringLiteral("gridRadius")));
        QVERIFY(t.contains(QStringLiteral("autoHideMinPresence")));
        QVERIFY(t.contains(QStringLiteral("iconMorphEnabled")));

        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.AppDeckRoot")));
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.AppDeckController")));
        QVERIFY(win.contains(QStringLiteral("id: appDeckRoot")));
    }

    void appDeck_v2_geometryExposesAnchorAPI()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString g = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckGeometry.qml");
        QVERIFY2(!g.isEmpty(), "AppDeckGeometry.qml unreadable");

        QVERIFY(g.contains(QStringLiteral("function dockAnchorFor")));
        QVERIFY(g.contains(QStringLiteral("function gridAnchorFor")));
        QVERIFY(g.contains(QStringLiteral("function pointLerp")));
        QVERIFY(g.contains(QStringLiteral("function lerp")));
        QVERIFY(g.contains(QStringLiteral("function freezeAnchors")));
        QVERIFY(g.contains(QStringLiteral("function unfreezeAnchors")));
        QVERIFY(g.contains(QStringLiteral("function recompute")));
        QVERIFY(g.contains(QStringLiteral("property rect dockRect")));
        QVERIFY(g.contains(QStringLiteral("property rect gridRect")));
        QVERIFY(g.contains(QStringLiteral("property var dockAnchorsById")));
        QVERIFY(g.contains(QStringLiteral("property var gridAnchorsByIndex")));

        const QString grid = readTextFile(base + "/Qml/components/appdeck/AppDeckGridView.qml");
        QVERIFY(grid.contains(QStringLiteral("function gridIconCenterFor")));
        QVERIFY(grid.contains(QStringLiteral("signal gridLayoutSettled")));

        const QString gridView = readTextFile(base + "/Qml/components/appdeck/AppDeckGridAppsView.qml");
        QVERIFY(gridView.contains(QStringLiteral("signal gridLayoutSettled")));
        QVERIFY(gridView.contains(QStringLiteral("onGridLayoutSettled:")));

        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.AppDeckGeometry")));
        QVERIFY(win.contains(QStringLiteral("id: appDeckGeometry")));
        QVERIFY(win.contains(QStringLiteral("appDeckGeometry.applyDockIconRects")));
    }

    void appDeck_v2_morphContainerBindsToProgress()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString m = readTextFile(base + "/Qml/components/appdeck/v2/MorphContainer.qml");
        QVERIFY2(!m.isEmpty(), "MorphContainer.qml unreadable");

        // §6: x/y/w/h/radius/color all lerp from source to target by morphProgress
        QVERIFY(m.contains(QStringLiteral("sourceRect.x")));
        QVERIFY(m.contains(QStringLiteral("targetRect.x")));
        QVERIFY(m.contains(QStringLiteral("sourceRadius")));
        QVERIFY(m.contains(QStringLiteral("targetRadius")));
        QVERIFY(m.contains(QStringLiteral("property real morphProgress")));
        QVERIFY(m.contains(QStringLiteral("_lerp(sourceRect.x")));
        QVERIFY(m.contains(QStringLiteral("_mix(sourceColor")));

        // §11: no per-property Behavior blocks inside MorphContainer
        QVERIFY(!m.contains(QStringLiteral("Behavior on x")));
        QVERIFY(!m.contains(QStringLiteral("Behavior on width")));
        QVERIFY(!m.contains(QStringLiteral("Behavior on radius")));

        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.MorphContainer")));
        QVERIFY(win.contains(QStringLiteral("id: surfaceMorph")));
        // §11: existing surfaceMorph block must no longer carry per-property
        // Behaviors — those have moved out into a single morphProgress driver.
        QVERIFY(!win.contains(QStringLiteral("Behavior on radius {")));
    }

    void appDeck_v2_iconMorphLayerScaffoldExists()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        QVERIFY(QFile::exists(base + "/Qml/components/appdeck/v2/IconMorphLayer.qml"));
        QVERIFY(QFile::exists(base + "/Qml/components/appdeck/v2/AppDeckIconDelegate.qml"));

        const QString l = readTextFile(base + "/Qml/components/appdeck/v2/IconMorphLayer.qml");
        // §7: single delegate per app, anchor lerp drives position, scale via tokens.
        QVERIFY(l.contains(QStringLiteral("AppDeckIconDelegate")));
        QVERIFY(l.contains(QStringLiteral("geometry.dockAnchorFor")));
        QVERIFY(l.contains(QStringLiteral("geometry.gridAnchorFor")));
        QVERIFY(l.contains(QStringLiteral("geometry.pointLerp")));
        QVERIFY(l.contains(QStringLiteral("dockIconScale")));
        QVERIFY(l.contains(QStringLiteral("gridIconScale")));

        // §7: dock and grid views expose iconsRenderedExternally hook
        const QString dock = readTextFile(base + "/Qml/components/appdeck/AppDeck.qml");
        QVERIFY(dock.contains(QStringLiteral("property bool iconsRenderedExternally")));
        const QString gridView = readTextFile(base + "/Qml/components/appdeck/AppDeckGridView.qml");
        QVERIFY(gridView.contains(QStringLiteral("property bool iconsRenderedExternally")));
        const QString collapsed = readTextFile(base + "/Qml/components/appdeck/AppDeckCollapsedView.qml");
        QVERIFY(collapsed.contains(QStringLiteral("property bool iconsRenderedExternally")));
        const QString gridApps = readTextFile(base + "/Qml/components/appdeck/AppDeckGridAppsView.qml");
        QVERIFY(gridApps.contains(QStringLiteral("property bool iconsRenderedExternally")));

        // §12: freeze/unfreeze hooks wired around the morph animation
        const QString r = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckRoot.qml");
        QVERIFY(r.contains(QStringLiteral("freezeAnchors()")));
        QVERIFY(r.contains(QStringLiteral("unfreezeAnchors()")));

        // §16 + Tahap 4 rollback: env flag wired into main.cpp + tokens
        const QString main = readTextFile(base + "/main.cpp");
        QVERIFY(main.contains(QStringLiteral("SLM_APPDECK_V2_ICON_MORPH")));
        QVERIFY(main.contains(QStringLiteral("AppDeckV2IconMorphEnabled")));
        const QString tokens = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckTokens.qml");
        QVERIFY(tokens.contains(QStringLiteral("iconMorphEnabled")));
        QVERIFY(tokens.contains(QStringLiteral("AppDeckV2IconMorphEnabled")));

        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.IconMorphLayer")));
        QVERIFY(win.contains(QStringLiteral("iconsRenderedExternally: appDeckRoot.tokens.iconMorphEnabled")));
    }

    void appDeck_v2_gridContentRevealHasStagger()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString g = readTextFile(base + "/Qml/components/appdeck/v2/GridContentLayer.qml");
        QVERIFY2(!g.isEmpty(), "GridContentLayer.qml unreadable");

        // §8: stagger curve formula — opacity 0→1 over revealStart..revealEnd
        QVERIFY(g.contains(QStringLiteral("property real revealStart")));
        QVERIFY(g.contains(QStringLiteral("property real revealEnd")));
        QVERIFY(g.contains(QStringLiteral("readonly property real revealOpacity")));
        QVERIFY(g.contains(QStringLiteral("readonly property real revealScale")));
        QVERIFY(g.contains(QStringLiteral("_t > visibleAt")));
        QVERIFY(g.contains(QStringLiteral("property bool gateVisible")));
        QVERIFY(g.contains(QStringLiteral("(_t - revealStart) / Math.max(0.001, revealEnd - revealStart)")));

        const QString gridApps = readTextFile(base + "/Qml/components/appdeck/AppDeckGridAppsView.qml");
        QVERIFY(gridApps.contains(QStringLiteral("property real morphProgress")));
        // GridContentLayer is now the wrapper for the header — Tahap 5 consumer migration.
        QVERIFY(gridApps.contains(QStringLiteral("AppDeckV2.GridContentLayer")));
        QVERIFY(gridApps.contains(QStringLiteral("id: headerHitLayer")));

        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(win.contains(QStringLiteral("morphProgress: appDeckRoot.morphProgress")));
    }

    void appDeck_v2_pulseAndContextAreModesNotWindows()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString p = readTextFile(base + "/Qml/components/appdeck/v2/PulseLayer.qml");
        QVERIFY2(!p.isEmpty(), "PulseLayer.qml unreadable");
        // §9: pulse is a mode, not a Window
        QVERIFY(!p.contains(QStringLiteral("Window {")));
        QVERIFY(p.contains(QStringLiteral("deckMode === \"pulse\"")));
        QVERIFY(p.contains(QStringLiteral("opacity: active ? 1.0 : 0.0")));

        const QString c = readTextFile(base + "/Qml/components/appdeck/v2/ContextLayer.qml");
        QVERIFY2(!c.isEmpty(), "ContextLayer.qml unreadable");
        QVERIFY(!c.contains(QStringLiteral("Window {")));
        QVERIFY(c.contains(QStringLiteral("deckMode === \"context\"")));
    }

    void appDeck_v2_compositorEventsAndMotionExist()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString h = readTextFile(base + "/src/core/wayland/layershell/appdeckcompositorevents.h");
        QVERIFY2(!h.isEmpty(), "appdeckcompositorevents.h unreadable");

        // §2.2: six named events
        QVERIFY(h.contains(QStringLiteral("void outsidePointerPressed(")));
        QVERIFY(h.contains(QStringLiteral("void appActivated(")));
        QVERIFY(h.contains(QStringLiteral("void workspaceFocused(")));
        QVERIFY(h.contains(QStringLiteral("void fullscreenStateChanged(")));
        QVERIFY(h.contains(QStringLiteral("void screenGeometryChanged(")));
        QVERIFY(h.contains(QStringLiteral("void scaleFactorChanged(")));

        const QString m = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckMotion.qml");
        QVERIFY2(!m.isEmpty(), "AppDeckMotion.qml unreadable");

        // §10: full motion API
        QVERIFY(m.contains(QStringLiteral("function expandToGrid")));
        QVERIFY(m.contains(QStringLiteral("function collapseToDock")));
        QVERIFY(m.contains(QStringLiteral("function switchMode")));
        QVERIFY(m.contains(QStringLiteral("function revealDock")));
        QVERIFY(m.contains(QStringLiteral("function hideDock")));

        const QString main = readTextFile(base + "/main.cpp");
        QVERIFY(main.contains(QStringLiteral("AppDeckCompositorEvents appDeckCompositorEvents")));
        QVERIFY(main.contains(QStringLiteral("\"AppDeckCompositorEvents\"")));

        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.AppDeckMotion")));
        QVERIFY(win.contains(QStringLiteral("id: appDeckMotion")));
    }

    void appDeck_v2_autoHideKeepsRevealZone()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString r = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckRoot.qml");
        QVERIFY2(!r.isEmpty(), "AppDeckRoot.qml unreadable");
        // §15: dockPresence is animated, not snapped
        QVERIFY(r.contains(QStringLiteral("property real dockPresence")));
        QVERIFY(r.contains(QStringLiteral("Behavior on dockPresence")));

        const QString t = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckTokens.qml");
        QVERIFY(t.contains(QStringLiteral("autoHideMinPresence")));

        // §15: AppDeckWindow must not set dockView.visible = false outright
        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(!win.contains(QStringLiteral("dockView.visible = false")));
    }

    void appDeck_v2_debugOverlayExistsAndIsWired()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString d = readTextFile(base + "/Qml/components/appdeck/v2/AppDeckDebugOverlay.qml");
        QVERIFY2(!d.isEmpty(), "AppDeckDebugOverlay.qml unreadable");
        QVERIFY(d.contains(QStringLiteral("Ctrl+Alt+D")));
        QVERIFY(d.contains(QStringLiteral("morphProgress")));

        const QString win = readTextFile(base + "/Qml/components/overlay/AppDeckWindow.qml");
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.AppDeckDebugOverlay")));
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
