import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: root

    property var workspaceModel: []
    property bool dragEnabled: false
    property bool dragInProgress: false
    signal workspaceActivated(int workspaceId)
    signal windowDroppedToWorkspace(string viewId, int workspaceId)

    radius: Theme.radiusControlLarge
    color: Theme.color("workspaceBarBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("workspaceBarBorder")

    Row {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Label {
            text: "Workspace"
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("bold")
            anchors.verticalCenter: parent.verticalCenter
        }

        Repeater {
            model: root.workspaceModel

            delegate: Rectangle {
                id: chip
                required property var modelData
                readonly property int workspaceId: Number((modelData && modelData.id) || (modelData && modelData.index) || 1)
                readonly property bool selected: !!(modelData && modelData.isActive)
                readonly property bool placeholder: !!(modelData && modelData.isTrailingEmpty)
                readonly property bool emptyWorkspace: !!(modelData && modelData.isEmpty)
                readonly property int workspaceWindows: Number((modelData && modelData.windowCount) || 0)
                readonly property bool hasWindows: workspaceWindows > 0
                readonly property bool isInactive: !selected && !placeholder && !emptyWorkspace
                property bool dropActive: false
                readonly property bool createTargetActive: placeholder && dropActive
                readonly property color chipColor: selected
                                                 ? Theme.color("accent")
                                                 : (placeholder
                                                    ? Theme.color("workspaceAddChipBg")
                                                    : Theme.color("workspaceChipBg"))
                readonly property color chipBorderColor: selected
                                                       ? Theme.color("selectedItemText")
                                                       : Theme.color("workspaceChipBorder")
                readonly property real chipOpacity: {
                    if (dropActive) {
                        return 1.0
                    }
                    if (placeholder) {
                        return 0.95
                    }
                    if (emptyWorkspace && !selected) {
                        return 0.84
                    }
                    if (isInactive && !hasWindows) {
                        return 0.90
                    }
                    return 1.0
                }

                width: placeholder ? 56 : 50
                height: 30
                radius: Theme.radiusControlLarge
                color: dropActive ? Theme.color("accent") : chipColor
                border.width: dropActive ? 2 : Theme.borderWidthThin
                border.color: dropActive ? Theme.color("selectedItemText") : chipBorderColor
                opacity: chipOpacity
                scale: dropActive ? 1.08 : 1.0
                Behavior on color { ColorAnimation { duration: Theme.durationSm } }
                Behavior on opacity { NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }
                Behavior on scale { NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: Theme.radiusControl
                    color: "transparent"
                    border.width: createTargetActive ? 1 : 0
                    border.color: Theme.color("selectedItemText")
                    opacity: createTargetActive ? 0.75 : 0.0
                    Behavior on opacity { NumberAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDecelerate } }
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 5

                    Label {
                        text: createTargetActive ? "New" : (placeholder ? "+" : ("S" + chip.workspaceId))
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: Theme.fontWeight("bold")
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        color: selected
                               ? Theme.color("selectedItemText")
                               : Theme.color("workspaceWindowCountBadge")
                        visible: !placeholder && hasWindows
                        anchors.verticalCenter: parent.verticalCenter

                        Label {
                            anchors.centerIn: parent
                            text: workspaceWindows > 9 ? "9+" : String(workspaceWindows)
                            color: selected ? Theme.color("accent") : Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("xxs")
                            font.weight: Theme.fontWeight("bold")
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.workspaceActivated(chip.workspaceId)
                }

                DropArea {
                    anchors.fill: parent
                    enabled: root.dragEnabled
                    keys: [ "workspaceWindowThumbnail" ]
                    onEntered: chip.dropActive = true
                    onExited: chip.dropActive = false
                    onDropped: function(drop) {
                        chip.dropActive = false
                        if (!drop || !drop.source) {
                            return
                        }
                        var viewId = String(drop.source.dragViewId || "")
                        if (!viewId.length) {
                            return
                        }
                        root.windowDroppedToWorkspace(viewId, chip.workspaceId)
                    }
                }
            }
        }
    }
}
