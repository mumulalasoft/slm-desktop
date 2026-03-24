import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    implicitHeight: contentLayout.implicitHeight + 24
    implicitWidth: parent ? parent.width : 200

    property alias label: labelText.text
    property alias description: descriptionText.text
    property bool showDescription: descriptionText.text !== ""
    property bool highlighted: false
    default property alias content: controlContainer.data

    // Highlighted-row background (replaces the old per-card border)
    Rectangle {
        anchors.fill: parent
        color: root.highlighted ? Theme.color("accentSoft") : "transparent"
        Behavior on color {
            ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
        }
    }

    RowLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                id: labelText
                font.pixelSize: Theme.fontSize("body")
                font.weight: Font.Medium
                color: Theme.color("textPrimary")
                Layout.fillWidth: true
            }

            Text {
                id: descriptionText
                font.pixelSize: Theme.fontSize("small")
                color: Theme.color("textSecondary")
                Layout.fillWidth: true
                visible: root.showDescription
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: controlContainer
            Layout.preferredWidth: implicitWidth
            Layout.fillHeight: true
            implicitWidth: childrenRect.width
        }
    }

    // Bottom separator — hidden when this is the last visible sibling
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        height: 1
        color: Theme.color("panelBorder")
        visible: {
            if (!root.parent) return false
            var seenVisible = false
            for (var i = root.parent.children.length - 1; i >= 0; i--) {
                var c = root.parent.children[i]
                if (c === root) return seenVisible
                if (c.visible) seenVisible = true
            }
            return false
        }
    }

    // Highlight pulse — flash the row briefly when deep-linked
    SequentialAnimation {
        id: highlightPulse
        running: false
        NumberAnimation {
            target: root; property: "opacity"
            from: 1.0; to: 0.6
            duration: 100; easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root; property: "opacity"
            from: 0.6; to: 1.0
            duration: 200; easing.type: Easing.OutCubic
        }
    }

    onHighlightedChanged: {
        if (highlighted) highlightPulse.restart()
    }
}
