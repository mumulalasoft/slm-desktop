import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: parent ? parent.width : implicitWidth

    property string titleText: ""
    property var items: []
    property string selectedResultId: ""
    property string sectionType: "item" // app | file | action | recent | suggestion
    property bool active: true
    property int revealDelayMs: 0

    signal itemHovered(string resultId)
    signal itemActivated(string resultId)
    signal itemContextAction(string resultId, string action)

    function itemCount() {
        return items && items.length !== undefined ? Number(items.length) : 0
    }

    visible: itemCount() > 0
    implicitHeight: sectionPlate.implicitHeight

    Rectangle {
        id: sectionPlate
        width: root.width
        implicitHeight: contentColumn.implicitHeight + 12
        radius: Theme.radiusCard
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.11) }
            GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.06) }
        }
        border.width: Theme.borderWidthNone

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 6
            spacing: 0

            Row {
                spacing: 6

                Label {
                    text: root.titleText
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                    font.weight: Theme.fontWeight("semibold")
                    opacity: Theme.opacityMuted
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "(" + root.itemCount() + ")"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("xs")
                    opacity: Theme.opacityMuted
                }
            }

            Item {
                width: 1
                height: 6
            }

            Repeater {
                model: (root.sectionType === "app"
                        || root.sectionType === "suggestion"
                        || root.sectionType === "clipboard") ? root.items : []
                PulseResultItem {
                    width: contentColumn.width
                    resultData: modelData
                    selected: String(modelData && modelData.resultId ? modelData.resultId : "") === root.selectedResultId
                    onHovered: function(resultId) { root.itemHovered(resultId) }
                    onActivated: function(resultId) { root.itemActivated(resultId) }
                    onContextActionRequested: function(resultId, action) {
                        root.itemContextAction(resultId, action)
                    }
                }
            }

            Repeater {
                model: (root.sectionType === "file" || root.sectionType === "recent") ? root.items : []
                PulseFileResultItem {
                    width: contentColumn.width
                    resultData: modelData
                    selected: String(modelData && modelData.resultId ? modelData.resultId : "") === root.selectedResultId
                    onHovered: function(resultId) { root.itemHovered(resultId) }
                    onActivated: function(resultId) { root.itemActivated(resultId) }
                    onContextActionRequested: function(resultId, action) {
                        root.itemContextAction(resultId, action)
                    }
                }
            }

            Repeater {
                model: (root.sectionType === "action") ? root.items : []
                PulseActionResultItem {
                    width: contentColumn.width
                    resultData: modelData
                    selected: String(modelData && modelData.resultId ? modelData.resultId : "") === root.selectedResultId
                    onHovered: function(resultId) { root.itemHovered(resultId) }
                    onActivated: function(resultId) { root.itemActivated(resultId) }
                    onContextActionRequested: function(resultId, action) {
                        root.itemContextAction(resultId, action)
                    }
                }
            }
        }
    }
}
