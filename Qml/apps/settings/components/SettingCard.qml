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
    property bool hideSeparator: false
    default property alias content: controlContainer.data

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    // Highlighted-row background
    Rectangle {
        id: highlightBg
        anchors.fill: parent
        color: "transparent"
    }

    RowLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                id: labelText
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("medium")
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
            if (root.hideSeparator) return false
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

    // Flash accent → accentSoft when deep-linked to this row
    ColorAnimation {
        id: highlightFlash
        target: highlightBg
        property: "color"
        from: Theme.color("accent")
        to: Theme.color("accentSoft")
        duration: Theme.durationMd
        easing.type: Theme.easingDefault
    }

    ColorAnimation {
        id: highlightFade
        target: highlightBg
        property: "color"
        to: "transparent"
        duration: Theme.durationSm
        easing.type: Theme.easingDefault
    }

    onHighlightedChanged: {
        if (highlighted) {
            highlightBg.color = Theme.color("accent")
            if (root.microAnimationAllowed()) {
                highlightFlash.restart()
            } else {
                highlightBg.color = Theme.color("accentSoft")
            }
        } else {
            if (root.microAnimationAllowed()) {
                highlightFade.restart()
            } else {
                highlightBg.color = "transparent"
            }
        }
    }
}
