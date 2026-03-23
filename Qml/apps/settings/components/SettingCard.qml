import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    height: contentLayout.implicitHeight + 24
    radius: 10
    color: "white"
    border.color: highlighted ? "#4f8ff7" : "#E5E5EA"
    border.width: 1
    property bool highlighted: false
    opacity: highlighted ? 1.0 : 0.98

    property alias label: labelText.text
    property alias description: descriptionText.text
    property bool showDescription: descriptionText.text !== ""
    default property alias content: controlContainer.data

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
                font.pixelSize: 14
                font.weight: Font.Medium
                color: "#1C1C1E"
                Layout.fillWidth: true
            }

            Text {
                id: descriptionText
                font.pixelSize: 12
                color: "#8E8E93"
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

    Behavior on border.color {
        ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    Behavior on opacity {
        NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
    }

    SequentialAnimation {
        id: highlightPulse
        running: false
        NumberAnimation { target: root; property: "scale"; from: 1.0; to: 1.012; duration: 120; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "scale"; from: 1.012; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
    }

    onHighlightedChanged: {
        if (highlighted) {
            highlightPulse.restart()
        }
    }
}
