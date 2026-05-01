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
    property string hostName: "appdeck"

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
        appsModel: root.appsModel
        Component.onCompleted: root.syncHideBorder()
        onAppActivated: function(appName) { root.appActivated(appName) }
        onAppdeckRequested: {
            root.appdeckRequested()
        }
    }
}
