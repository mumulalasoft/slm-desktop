import QtQuick 2.15

Column {
    id: root

    property bool active: false
    property string firstTitle: ""
    property var firstItems: []
    property string firstType: "item"
    property int firstDelayMs: 0

    property string secondTitle: ""
    property var secondItems: []
    property string secondType: "item"
    property int secondDelayMs: 24

    property string selectedResultId: ""

    signal itemHovered(string resultId)
    signal itemActivated(string resultId)
    signal itemContextAction(string resultId, string action)

    spacing: 10
    visible: (firstItems && firstItems.length > 0)
             || (secondItems && secondItems.length > 0)

    PulseResultsSection {
        width: root.width
        titleText: root.firstTitle
        items: root.firstItems
        sectionType: root.firstType
        active: root.active
        revealDelayMs: root.firstDelayMs
        selectedResultId: root.selectedResultId
        onItemHovered: function(resultId) { root.itemHovered(resultId) }
        onItemActivated: function(resultId) { root.itemActivated(resultId) }
        onItemContextAction: function(resultId, action) {
            root.itemContextAction(resultId, action)
        }
    }

    PulseResultsSection {
        width: root.width
        titleText: root.secondTitle
        items: root.secondItems
        sectionType: root.secondType
        active: root.active
        revealDelayMs: root.secondDelayMs
        selectedResultId: root.selectedResultId
        onItemHovered: function(resultId) { root.itemHovered(resultId) }
        onItemActivated: function(resultId) { root.itemActivated(resultId) }
        onItemContextAction: function(resultId, action) {
            root.itemContextAction(resultId, action)
        }
    }
}
