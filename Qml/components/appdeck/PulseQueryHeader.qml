import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    property bool active: false
    property string queryText: ""
    property string placeholderText: "Search apps, files, actions, commands"
    readonly property string pulseIconTheme: (typeof Theme !== "undefined" && Theme && Theme.darkMode) ? "dark" : "light"
    readonly property string pulseIconSource: "qrc:/icons/" + pulseIconTheme + "/pulse.svg"

    signal queryEdited(string text)
    signal clearRequested()
    signal closeRequested()
    signal navigate(int delta)
    signal navigateSection(int delta)
    signal activateCurrent()
    signal escapePressed()

    function focusInput() {
        field.forceActiveFocus()
        field.selectAll()
    }

    implicitHeight: 64
    opacity: active ? 1.0 : 0.0
    y: active ? 0 : 6

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
    }
    Behavior on y {
        NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusWindow
        color: Qt.rgba(1, 1, 1, 0.06)
        border.width: Theme.borderWidthThin
        border.color: field.activeFocus ? Theme.color("panelBorderStrong") : Theme.color("pulseQueryBorder")
        opacity: 1.0

        Behavior on border.color {
            ColorAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDecelerate }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: Theme.radiusPill
            color: field.activeFocus ? Theme.color("pulseBadgeActive") : Theme.color("accentSoft")
            border.width: Theme.borderWidthThin
            border.color: field.activeFocus ? Theme.color("panelBorderStrong") : Theme.color("panelBorder")
            scale: field.activeFocus ? 1.0 : 0.97

            Behavior on color {
                ColorAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDecelerate }
            }
            Behavior on scale {
                NumberAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDecelerate }
            }

            Image {
                id: pulseIcon
                anchors.centerIn: parent
                width: Theme.controlHeightCompact
                height: width
                source: root.pulseIconSource
                fillMode: Image.PreserveAspectFit
                mipmap: false
                smooth: true
            }

            Label {
                anchors.centerIn: parent
                visible: pulseIcon.status === Image.Error
                text: "P"
                color: Theme.color("accent")
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("semibold")
            }
        }

        TextField {
            id: field
            Layout.fillWidth: true
            text: root.queryText
            placeholderText: root.placeholderText
            placeholderTextColor: Theme.color("textSecondary")
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("body")
            background: null
            selectionColor: Theme.color("accent")
            selectedTextColor: Theme.color("accentText")

            onTextEdited: root.queryEdited(text)

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Up) {
                    root.navigate(-1)
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Down) {
                    root.navigate(1)
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.activateCurrent()
                    event.accepted = true
                    return
                }
                if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right)
                        && (event.modifiers & Qt.AltModifier)) {
                    root.navigateSection(event.key === Qt.Key_Left ? -1 : 1)
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Escape) {
                    root.escapePressed()
                    event.accepted = true
                }
            }
        }

        Rectangle {
            visible: field.text.length > 0
            Layout.preferredHeight: 34
            Layout.preferredWidth: clearLabel.implicitWidth + 20
            radius: Theme.radiusPill
            color: clearHover.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.08)
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            Label {
                id: clearLabel
                anchors.centerIn: parent
                text: "Clear"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("medium")
            }

            MouseArea {
                id: clearHover
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.clearRequested()
            }
        }

        Rectangle {
            Layout.preferredHeight: 34
            Layout.preferredWidth: escLabel.implicitWidth + 20
            radius: Theme.radiusPill
            color: escHover.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.08)
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            Label {
                id: escLabel
                anchors.centerIn: parent
                text: "Esc"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("medium")
            }

            MouseArea {
                id: escHover
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.closeRequested()
            }
        }
    }
}
