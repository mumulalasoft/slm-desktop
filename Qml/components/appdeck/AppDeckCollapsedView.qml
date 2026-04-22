import QtQuick 2.15
import "." as AppDeckComp
import "zones" as Zones

Item {
    id: root

    required property var appsModel

    property bool acceptsInput: true
    property bool rendererActive: true
    property string hostName: "appdeck"

    readonly property alias dockItem: dockSurface

    signal appActivated(string appName)
    signal apphubRequested()

    // Legacy behavior mapping:
    // collapsed = quick access (pinned + running indicators).
    Zones.FavoritesZone {
        id: favoritesZone
        appsModel: root.appsModel
    }

    Zones.RunningZone {
        id: runningZone
        appsModel: root.appsModel
    }

    AppDeckComp.AppDeck {
        id: dockSurface
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        hostName: root.hostName
        acceptsInput: root.acceptsInput
        rendererActive: root.rendererActive
        appsModel: root.appsModel
        onAppActivated: function(appName) { root.appActivated(appName) }
        onApphubRequested: root.apphubRequested()
    }
}
