import QtQuick 2.15
import "." as AppDeckComp

Item {
    id: root

    required property var appsModel

    property bool acceptsInput: true
    property bool rendererActive: true
    property bool hideBorder: false
    property string hostName: "appdeck"

    readonly property alias dockItem: dockSurface

    signal appActivated(string appName)
    signal apphubRequested()

    function syncHideBorder() {
        if (!dockSurface || !dockSurface.hasOwnProperty) {
            return
        }
        if (dockSurface.hasOwnProperty("hideBackgroundBorder")) {
            dockSurface.hideBackgroundBorder = root.hideBorder
        } else if (dockSurface.hasOwnProperty("hideBorder")) {
            dockSurface.hideBorder = root.hideBorder
        }
    }

    onHideBorderChanged: syncHideBorder()

    AppDeckComp.AppDeck {
        id: dockSurface
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        hostName: root.hostName
        acceptsInput: root.acceptsInput
        rendererActive: root.rendererActive
        appsModel: root.appsModel
        Component.onCompleted: root.syncHideBorder()
        onAppActivated: function(appName) { root.appActivated(appName) }
        onApphubRequested: {
            root.apphubRequested()
        }
    }
}
