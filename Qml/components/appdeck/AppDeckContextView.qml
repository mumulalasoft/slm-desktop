import QtQuick 2.15
import QtQuick.Controls 2.15
import "../pulse" as PulseComp
import "zones" as Zones

Item {
    id: root

    property bool active: false
    property int panelHeight: 0
    property string queryText: ""
    property var resultsModel: null
    property int selectedIndex: -1
    property bool showDebugInfo: false
    property var searchProfileMeta: ({})
    property var telemetryMeta: ({})
    property var telemetryLast: ({})
    property var providerStats: ({})
    property var previewData: ({})

    signal dismissRequested()
    signal queryTextChangedRequest(string text)
    signal selectedIndexChangedByUser(int indexValue)
    signal resultActivated(int indexValue)
    signal resultContextAction(int indexValue, string action)

    function focusSearchField() {
        if (pulseContent && pulseContent.focusSearch) {
            pulseContent.focusSearch()
        }
    }

    anchors.fill: parent
    visible: active

    MouseArea {
        anchors.fill: parent
        enabled: root.active
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: function(mouse) {
            var pt = mapToItem(contextCard, mouse.x, mouse.y)
            var inside = pt.x >= 0 && pt.y >= 0
                         && pt.x <= contextCard.width
                         && pt.y <= contextCard.height
            if (!inside) {
                root.dismissRequested()
                mouse.accepted = true
            }
        }
    }

    Rectangle {
        id: contextCard
        width: Math.min(760, Math.max(540, root.width - 120))
        height: Math.min(520, Math.max(360, root.height - 180))
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(root.panelHeight + 12, Math.round(root.height * 0.08))
        radius: Theme.radiusWindow
        color: "transparent"
        opacity: root.active ? 1.0 : 0.0
        scale: root.active ? 1.0 : 0.97

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingLight
            }
        }

        Behavior on scale {
            NumberAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingLight
            }
        }

        Zones.ContextZone {
            anchors.fill: parent

            PulseComp.PulseOverlay {
                id: pulseContent
                anchors.fill: parent
                active: root.active
                focus: active
                compactWindow: true
                showDebugInfo: root.showDebugInfo
                searchProfileMeta: root.searchProfileMeta
                telemetryMeta: root.telemetryMeta
                telemetryLast: root.telemetryLast
                providerStats: root.providerStats
                previewData: root.previewData
                queryText: root.queryText
                resultsModel: root.resultsModel
                selectedIndex: root.selectedIndex
                onQueryTextChangedRequest: function(text) {
                    root.queryTextChangedRequest(text)
                }
                onSelectedIndexChanged: {
                    root.selectedIndexChangedByUser(selectedIndex)
                }
                onResultActivated: function(indexValue) {
                    root.resultActivated(indexValue)
                }
                onResultContextAction: function(indexValue, action) {
                    root.resultContextAction(indexValue, action)
                }
                onDismissRequested: root.dismissRequested()
            }
        }
    }
}
