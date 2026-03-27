import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

// DeveloperNavPane — secondary sidebar for Settings > Developer.
//
// Always visible regardless of developer mode state; the Developer Mode toggle
// lives here so users can enable it without first having to find the right page.

Rectangle {
    id: root

    color: Theme.color("surface")
    border.color: Theme.color("panelBorder")
    border.width: 1

    // ── Public interface ──────────────────────────────────────────────────────

    property string currentPageId: "overview"
    property var    visiblePages:  []

    signal pageSelected(string pageId)

    // ── Layout ────────────────────────────────────────────────────────────────

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        // Developer Mode + Experimental toggles
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("Developer Mode")
                font.pixelSize: Theme.fontSize("xs")
                font.weight: Font.DemiBold
                color: Theme.color("textSecondary")
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Switch {
                implicitHeight: 20
                checked: DeveloperMode ? DeveloperMode.developerMode : false
                onToggled: if (DeveloperMode) DeveloperMode.developerMode = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: DeveloperMode && DeveloperMode.developerMode

            Text {
                text: qsTr("Experimental")
                font.pixelSize: Theme.fontSize("xs")
                color: Theme.color("textSecondary")
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Switch {
                implicitHeight: 20
                checked: DeveloperMode ? DeveloperMode.experimentalFeatures : false
                onToggled: if (DeveloperMode) DeveloperMode.experimentalFeatures = checked
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.color("panelBorder")
            Layout.topMargin: 4
            Layout.bottomMargin: 4
        }

        // Sub-page list
        ListView {
            id: pageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 1
            model: root.visiblePages

            section.property: "group"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                width: pageList.width
                height: section.length > 0 ? 22 : 0
                visible: section.length > 0
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: section
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    color: Theme.color("textDisabled")
                }
            }

            delegate: ItemDelegate {
                width: pageList.width
                height: 30
                highlighted: root.currentPageId === modelData.id

                background: Rectangle {
                    radius: 5
                    color: highlighted
                        ? Theme.color("accent")
                        : (hovered ? Theme.color("controlBgHover") : "transparent")
                    Behavior on color {
                        ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }
                }

                contentItem: RowLayout {
                    spacing: 6
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6

                    Image {
                        source: "image://icon/" + modelData.icon
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.label
                        font.pixelSize: Theme.fontSize("sm")
                        color: highlighted ? Theme.color("accentText") : Theme.color("textPrimary")
                        elide: Text.ElideRight
                        Behavior on color {
                            ColorAnimation { duration: 100 }
                        }
                    }
                }

                onClicked: root.pageSelected(modelData.id)
            }
        }
    }
}
