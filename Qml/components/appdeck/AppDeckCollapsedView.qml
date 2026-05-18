import QtQuick 2.15
import "." as AppDeckComp

Item {
    id: root

    required property var appsModel

    property bool acceptsInput: true
    property bool rendererActive: true
    property bool renderEffectsEnabled: true
    property bool hideBorder: false
    property bool transparentBackground: false
    property bool ignoreDesktopTransparentSetting: false
    property real backgroundMorphProgress: 0.0
    property string hostName: "appdeck"

    // Without explicit dimensions this Item would default to width=0, height=0.
    // QQuickItem hit-testing skips zero-sized items, which means clicks on the
    // dock pill in grid state fall through to the AppDeckGridAppsView outer
    // MouseArea (which treats them as "outside content" and dismisses the
    // grid). Sizing the container to match the inner AppDeck makes the dock
    // hit-testable so the launcher button can toggle grid in either state.
    implicitWidth: dockSurface ? dockSurface.width : 0
    implicitHeight: dockSurface ? dockSurface.height : 0
    width: implicitWidth
    height: implicitHeight
    // docs/APPDECK.md §7 — forwarded to AppDeck so IconMorphLayer can take
    // over icon painting while the dock chrome stays visible.
    property bool iconsRenderedExternally: false

    readonly property alias dockItem: dockSurface

    signal appActivated(string appName)
    signal appdeckRequested()

    function syncHideBorder() {
        if (!dockSurface || !dockSurface.hasOwnProperty) {
            return
        }
        if (dockSurface.hasOwnProperty("hideBackgroundBorder")) {
            dockSurface.hideBackgroundBorder = root.hideBorder
        } else if (dockSurface.hasOwnProperty("hideBorder")) {
            dockSurface.hideBorder = root.hideBorder
        }
        if (dockSurface.hasOwnProperty("forceTransparentBackground")) {
            dockSurface.forceTransparentBackground = root.transparentBackground
        }
    }

    onHideBorderChanged: syncHideBorder()
    onTransparentBackgroundChanged: syncHideBorder()

    AppDeckComp.AppDeck {
        id: dockSurface
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        hostName: root.hostName
        acceptsInput: root.acceptsInput
        rendererActive: root.rendererActive
        renderEffectsEnabled: root.renderEffectsEnabled
        forceTransparentBackground: root.transparentBackground
        ignoreDesktopTransparentSetting: root.ignoreDesktopTransparentSetting
        backgroundMorphProgress: root.backgroundMorphProgress
        iconsRenderedExternally: root.iconsRenderedExternally
        appsModel: root.appsModel
        Component.onCompleted: root.syncHideBorder()
        onAppActivated: function(appName) { root.appActivated(appName) }
        onAppdeckRequested: {
            root.appdeckRequested()
        }
        onWidthChanged: console.info(
            "[APPDECK-COLLAPSED-VIEW-SIZE] dockSurface.width=" + width
            + " collapsedView.width=" + root.width
            + " parent.width=" + (root.parent ? root.parent.width : "n/a"))
    }
}
