import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop

Rectangle {
    id: root
    color: Theme.color("surface")
    border.color: Theme.color("panelBorder")
    border.width: 1

    property alias query: searchField.text
    property var moduleModel: []
    property string currentModuleId: ""
    signal moduleSelected(string id)
    signal queryChangedByUser(string text)

    function forceSearchFocus() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    // macOS-style traffic-light button
    component TitleButton: Item {
        required property string normalSrc
        required property string hoverSrc
        required property string activeSrc
        signal clicked()

        implicitWidth: 14
        implicitHeight: 14

        property bool _hovered: false
        property bool _pressed: false

        Image {
            anchors.fill: parent
            source: parent._pressed ? parent.activeSrc
                                    : (parent._hovered ? parent.hoverSrc : parent.normalSrc)
            fillMode: Image.PreserveAspectFit
            smooth: true
            antialiasing: true
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            onEntered:   { parent._hovered = true }
            onExited:    { parent._hovered = false; parent._pressed = false }
            onPressed:   { parent._pressed = true }
            onReleased:  { parent._pressed = false }
            onClicked:   parent.clicked()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ── Window controls + drag handle ────────────────────────────────
        Item {
            Layout.fillWidth: true
            height: 28

            // Drag the window by this strip (DragHandler ignores quick taps on buttons)
            DragHandler {
                target: null
                onActiveChanged: {
                    if (active) {
                        const w = root.ApplicationWindow.window
                        if (w) w.startSystemMove()
                    }
                }
            }

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                TitleButton {
                    normalSrc: "qrc:/icons/titlebuttons/titlebutton-close.svg"
                    hoverSrc:  "qrc:/icons/titlebuttons/titlebutton-close-hover.svg"
                    activeSrc: "qrc:/icons/titlebuttons/titlebutton-close-active.svg"
                    onClicked: {
                        const w = root.ApplicationWindow.window
                        if (w) w.close()
                    }
                }

                TitleButton {
                    normalSrc: "qrc:/icons/titlebuttons/titlebutton-minimize.svg"
                    hoverSrc:  "qrc:/icons/titlebuttons/titlebutton-minimize-hover.svg"
                    activeSrc: "qrc:/icons/titlebuttons/titlebutton-minimize-active.svg"
                    onClicked: {
                        const w = root.ApplicationWindow.window
                        if (w) w.showMinimized()
                    }
                }

                TitleButton {
                    property bool isMax: {
                        const w = root.ApplicationWindow.window
                        return w !== null && w.visibility === Window.Maximized
                    }
                    normalSrc: "qrc:/icons/titlebuttons/titlebutton-maximize.svg"
                    hoverSrc:  isMax ? "qrc:/icons/titlebuttons/titlebutton-unmaximize-hover.svg"
                                     : "qrc:/icons/titlebuttons/titlebutton-maximize-hover.svg"
                    activeSrc: isMax ? "qrc:/icons/titlebuttons/titlebutton-unmaximize-active.svg"
                                     : "qrc:/icons/titlebuttons/titlebutton-maximize-active.svg"
                    onClicked: {
                        const w = root.ApplicationWindow.window
                        if (!w) return
                        if (isMax) w.showNormal(); else w.showMaximized()
                    }
                }
            }
        }

        // ── Search ───────────────────────────────────────────────────────
        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search settings")
            selectByMouse: true
            onTextChanged: root.queryChangedByUser(text)
            background: Rectangle {
                radius: 8
                color: Theme.color("windowBg")
                border.color: searchField.activeFocus ? Theme.color("accent") : Theme.color("panelBorder")
                border.width: searchField.activeFocus ? 2 : 1
            }
        }

        // ── Module list ──────────────────────────────────────────────────
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 1
            model: root.moduleModel
            section.property: "group"
            section.criteria: ViewSection.FullString
            section.delegate: Rectangle {
                width: listView.width
                height: 22
                color: "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: section
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    color: Theme.color("textSecondary")
                    textFormat: Text.PlainText
                }
            }

            delegate: ItemDelegate {
                width: listView.width
                height: 34
                highlighted: root.currentModuleId === (modelData.id || "")
                background: Rectangle {
                    radius: 6
                    color: highlighted
                        ? Theme.color("accent")
                        : (hovered ? Theme.color("controlBgHover") : "transparent")
                    Behavior on color {
                        ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
                    }
                }

                contentItem: RowLayout {
                    spacing: 8
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Image {
                        source: "image://icon/" + (modelData.icon || "preferences-system")
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.name || ""
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: highlighted ? Font.DemiBold : Font.Normal
                        color: highlighted ? Theme.color("accentText") : Theme.color("textPrimary")
                        elide: Text.ElideRight
                        Behavior on color {
                            ColorAnimation { duration: 120; easing.type: Easing.OutQuad }
                        }
                    }
                }

                onClicked: root.moduleSelected(modelData.id || "")
            }
        }
    }
}
