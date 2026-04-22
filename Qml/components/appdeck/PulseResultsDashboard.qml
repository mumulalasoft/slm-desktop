import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    property var appsResults: []
    property var filesResults: []
    property var actionsResults: []
    property var recentResults: []
    property var suggestedResults: []
    property string selectedResultId: ""
    property int twoColumnThreshold: 980
    property real layoutWidth: 0
    property bool active: false

    signal itemHovered(string resultId)
    signal itemActivated(string resultId)
    signal itemContextAction(string resultId, string action)

    readonly property real effectiveWidth: {
        var w = Number(width || 0)
        var lw = Number(layoutWidth || 0)
        if (w > 0 && lw > 0) {
            return Math.min(w, lw)
        }
        return w > 0 ? w : lw
    }
    readonly property bool twoColumnMode: effectiveWidth >= twoColumnThreshold
    readonly property var rightSecondaryItems: (recentResults && recentResults.length > 0)
                                               ? recentResults : suggestedResults
    readonly property string rightSecondaryTitle: (recentResults && recentResults.length > 0)
                                                 ? "Recent" : "Suggestions"

    implicitHeight: content.implicitHeight
    opacity: active ? 1.0 : 0.0
    y: active ? 0 : 8

    Behavior on opacity {
        NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
    }
    Behavior on y {
        NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 10

        Row {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(leftColumn.implicitHeight, rightColumn.implicitHeight)
            spacing: 12
            visible: root.twoColumnMode

            PulseResultsColumn {
                id: leftColumn
                width: Math.max(220, (parent.width - parent.spacing) * 0.5)
                firstTitle: "Applications"
                firstItems: root.appsResults
                firstType: "app"
                secondTitle: "Actions"
                secondItems: root.actionsResults
                secondType: "action"
                active: root.active
                firstDelayMs: 20
                secondDelayMs: 60
                selectedResultId: root.selectedResultId
                onItemHovered: function(resultId) { root.itemHovered(resultId) }
                onItemActivated: function(resultId) { root.itemActivated(resultId) }
                onItemContextAction: function(resultId, action) {
                    root.itemContextAction(resultId, action)
                }
            }

            PulseResultsColumn {
                id: rightColumn
                width: Math.max(220, (parent.width - parent.spacing) * 0.5)
                firstTitle: "Files"
                firstItems: root.filesResults
                firstType: "file"
                secondTitle: root.rightSecondaryTitle
                secondItems: root.rightSecondaryItems
                secondType: "recent"
                active: root.active
                firstDelayMs: 40
                secondDelayMs: 80
                selectedResultId: root.selectedResultId
                onItemHovered: function(resultId) { root.itemHovered(resultId) }
                onItemActivated: function(resultId) { root.itemActivated(resultId) }
                onItemContextAction: function(resultId, action) {
                    root.itemContextAction(resultId, action)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: !root.twoColumnMode

            PulseResultsSection {
                Layout.fillWidth: true
                titleText: "Applications"
                items: root.appsResults
                sectionType: "app"
                active: root.active
                revealDelayMs: 20
                selectedResultId: root.selectedResultId
                onItemHovered: function(resultId) { root.itemHovered(resultId) }
                onItemActivated: function(resultId) { root.itemActivated(resultId) }
                onItemContextAction: function(resultId, action) {
                    root.itemContextAction(resultId, action)
                }
            }

            PulseResultsSection {
                Layout.fillWidth: true
                titleText: "Files"
                items: root.filesResults
                sectionType: "file"
                active: root.active
                revealDelayMs: 40
                selectedResultId: root.selectedResultId
                onItemHovered: function(resultId) { root.itemHovered(resultId) }
                onItemActivated: function(resultId) { root.itemActivated(resultId) }
                onItemContextAction: function(resultId, action) {
                    root.itemContextAction(resultId, action)
                }
            }

            PulseResultsSection {
                Layout.fillWidth: true
                titleText: "Actions"
                items: root.actionsResults
                sectionType: "action"
                active: root.active
                revealDelayMs: 60
                selectedResultId: root.selectedResultId
                onItemHovered: function(resultId) { root.itemHovered(resultId) }
                onItemActivated: function(resultId) { root.itemActivated(resultId) }
                onItemContextAction: function(resultId, action) {
                    root.itemContextAction(resultId, action)
                }
            }

            PulseResultsSection {
                Layout.fillWidth: true
                titleText: "Recent"
                items: root.recentResults
                sectionType: "recent"
                active: root.active
                revealDelayMs: 80
                selectedResultId: root.selectedResultId
                onItemHovered: function(resultId) { root.itemHovered(resultId) }
                onItemActivated: function(resultId) { root.itemActivated(resultId) }
                onItemContextAction: function(resultId, action) {
                    root.itemContextAction(resultId, action)
                }
            }

            PulseResultsSection {
                Layout.fillWidth: true
                titleText: "Suggestions"
                items: root.suggestedResults
                sectionType: "suggestion"
                active: root.active
                revealDelayMs: 100
                selectedResultId: root.selectedResultId
                onItemHovered: function(resultId) { root.itemHovered(resultId) }
                onItemActivated: function(resultId) { root.itemActivated(resultId) }
                onItemContextAction: function(resultId, action) {
                    root.itemContextAction(resultId, action)
                }
            }
        }
    }

    onTwoColumnModeChanged: {
        console.log("[appdeck-pulse] dashboard twoColumn=", twoColumnMode,
                    "width=", Math.round(width),
                    "effectiveWidth=", Math.round(effectiveWidth),
                    "threshold=", twoColumnThreshold)
    }

    Component.onCompleted: {
        console.log("[appdeck-pulse] dashboard init twoColumn=", twoColumnMode,
                    "width=", Math.round(width),
                    "effectiveWidth=", Math.round(effectiveWidth),
                    "threshold=", twoColumnThreshold)
    }
}
