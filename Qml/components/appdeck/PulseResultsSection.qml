import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: parent ? parent.width : implicitWidth

    property string titleText: ""
    property var items: []
    property string selectedResultId: ""
    property string sectionType: "item" // app | file | action | recent | suggestion | workflow | clipboard
    property bool active: true
    property int revealDelayMs: 0

    signal itemHovered(string resultId)
    signal itemActivated(string resultId)
    signal itemContextAction(string resultId, string action)

    // docs/APPDECK.md §19 — Progressive reveal stagger. Sections fade+slide
    // in with a per-section delay so the dashboard "emerges" rather than
    // popping. itemsLength is reactive (function calls don't drive bindings)
    // so the reveal also fires when results arrive late (per §17 incremental
    // updates).
    readonly property int itemsLength: items && items.length !== undefined
                                       ? Number(items.length) : 0
    readonly property bool _shouldReveal: active && itemsLength > 0
    property bool _revealed: false
    property real _revealOffset: _revealed ? 0 : 6

    function itemCount() {
        return itemsLength
    }

    visible: itemsLength > 0
    implicitHeight: sectionPlate.implicitHeight
    opacity: _revealed ? 1.0 : 0.0
    transform: Translate { y: root._revealOffset }

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationNormal; easing.type: Theme.easingDecelerate }
    }
    Behavior on _revealOffset {
        NumberAnimation { duration: Theme.durationNormal; easing.type: Theme.easingDecelerate }
    }

    Timer {
        id: stagger
        interval: root.revealDelayMs
        repeat: false
        onTriggered: root._revealed = true
    }

    on_ShouldRevealChanged: {
        if (_shouldReveal) {
            _revealed = false
            stagger.restart()
        } else {
            stagger.stop()
            _revealed = false
        }
    }

    Component.onCompleted: {
        if (_shouldReveal) {
            stagger.restart()
        }
    }

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
                // §18 — workflows are action-shaped (commands the user can
                // resume / launch) so they ride on PulseActionResultItem.
                model: (root.sectionType === "action"
                        || root.sectionType === "workflow") ? root.items : []
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
