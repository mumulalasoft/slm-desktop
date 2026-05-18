// AppDeck v2 context mode layer (docs/APPDECK.md §9).
// Context (file/app context menu actions) is a MODE within state=grid, not
// a separate Window. Cross-fades on opacity while morphProgress stays 1.0.
import QtQuick 2.15
import Slm_Desktop
import ".." as AppDeckComp

Item {
    id: contextLayer
    anchors.fill: parent

    property string deckMode: "apps"
    property var pulseResultsModel: null
    property real contextSurfaceX: 0
    property real contextSurfaceY: 0
    property real contextSurfaceW: 0
    property real contextSurfaceH: 0

    readonly property bool active: deckMode === "context"

    signal collapseRequested()

    opacity: active ? 1.0 : 0.0
    visible: opacity > 0.01

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationMd; easing.type: Theme.easingLight }
    }

    AppDeckComp.AppDeckContextView {
        id: ctxView
        anchors.fill: parent
        pulseResultsModel: contextLayer.pulseResultsModel
        active: contextLayer.active
        onCollapseRequested: contextLayer.collapseRequested()
    }
}
