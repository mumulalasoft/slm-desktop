import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    property var appsResults: []
    property var filesResults: []
    property var clipboardResults: []
    property var actionsResults: []
    property var recentResults: []
    property var suggestedResults: []
    // docs/APPDECK.md §18 — Workflows is part of the canonical grouped
    // sections. Empty by default; pulse providers can populate via the same
    // dashboard wiring used for actions/suggestions.
    property var workflowsResults: []
    property string selectedResultId: ""
    property int twoColumnThreshold: 980
    property real layoutWidth: 0
    property bool active: false
    readonly property int motionDuration: Theme.durationFast

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
    // §18 — Two-column ONLY when the right column would actually have
    // content. Without this, empty Files+Recent leaves a wide blank pocket
    // alongside Apps+Actions in the left column.
    readonly property bool _rightHasContent: (filesResults && filesResults.length > 0)
                                             || (recentResults && recentResults.length > 0)
                                             || (suggestedResults && suggestedResults.length > 0)
    readonly property bool twoColumnMode: effectiveWidth >= twoColumnThreshold
                                          && _rightHasContent
    readonly property var rightSecondaryItems: (recentResults && recentResults.length > 0)
                                               ? recentResults : suggestedResults
    readonly property string rightSecondaryTitle: (recentResults && recentResults.length > 0)
                                                 ? "Recent" : "Suggestions"

    implicitHeight: content.implicitHeight
    // docs/APPDECK.md §15 — Intent emergence: pulse dashboard scales up from
    // its top edge (search field anchor direction) and slides down a few
    // pixels so it reads as "Pulse emerging from Grid" rather than a popup
    // appearing in mid-air. Transform origin is top-center so the scale
    // grows downward from the search pill above.
    opacity: active ? 1.0 : 0.0
    y: active ? 0 : -6
    scale: active ? 1.0 : 0.97
    transformOrigin: Item.Top

    Behavior on opacity {
        NumberAnimation { duration: root.motionDuration; easing.type: Theme.easingDecelerate }
    }
    // docs/APPDECK.md §15/§19 — Spring easing on the geometric emergence
    // (y + scale) so the dashboard settles with a gentle overshoot per the
    // "intent emergence" feel. Opacity stays linear-decelerate (overshoot
    // on alpha is not visually meaningful).
    Behavior on y {
        NumberAnimation {
            duration: root.motionDuration
            easing.type: Theme.easingSpring
            easing.overshoot: 1.2
        }
    }
    Behavior on scale {
        NumberAnimation {
            duration: root.motionDuration
            easing.type: Theme.easingSpring
            easing.overshoot: 1.2
        }
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

        PulseResultsSection {
            Layout.fillWidth: true
            titleText: "Clipboard"
            items: root.clipboardResults
            sectionType: "clipboard"
            active: root.active
            revealDelayMs: 72
            selectedResultId: root.selectedResultId
            visible: root.twoColumnMode && items && items.length > 0
            onItemHovered: function(resultId) { root.itemHovered(resultId) }
            onItemActivated: function(resultId) { root.itemActivated(resultId) }
            onItemContextAction: function(resultId, action) {
                root.itemContextAction(resultId, action)
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
                titleText: "Clipboard"
                items: root.clipboardResults
                sectionType: "clipboard"
                active: root.active
                revealDelayMs: 72
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
                titleText: "Workflows"
                items: root.workflowsResults
                sectionType: "workflow"
                active: root.active
                revealDelayMs: 90
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
                revealDelayMs: 110
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
