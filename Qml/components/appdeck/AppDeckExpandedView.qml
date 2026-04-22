import QtQuick 2.15
import "../apphub" as AppHubComp
import "zones" as Zones

Item {
    id: root

    required property var appsModel
    required property var desktopScene

    property int panelHeight: 0
    property int bottomSafeInset: 160
    property string apphubSearchSeed: ""

    signal dismissRequested()
    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    anchors.fill: parent

    Zones.SuggestedZone {
        id: suggestedZone
        anchors.fill: parent
        appsModel: root.appsModel
    }

    AppHubComp.AppHub {
        id: apphubContent
        anchors.fill: parent
        desktopScene: root.desktopScene
        appsModel: root.appsModel
        externalSearchSeed: root.apphubSearchSeed
        topSafeInset: root.panelHeight
        bottomSafeInset: root.bottomSafeInset
        revealProgress: 1.0
        onDismissRequested: root.dismissRequested()
        onAppChosen: function(appData) { root.appChosen(appData) }
        onAddToDockRequested: function(appData) { root.addToDockRequested(appData) }
        onAddToDesktopRequested: function(appData) { root.addToDesktopRequested(appData) }
    }
}
