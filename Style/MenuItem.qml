import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import Slm_Desktop

T.MenuItem {
    id: control

    readonly property bool hasSubMenu: !!control.subMenu
    property string iconSource: ""
    property string secondaryText: ""
    property string shortcutText: ""

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(26,
                             implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    topPadding: 1
    bottomPadding: 1
    leftPadding: 7
    rightPadding: 7
    spacing: 6
    font.family: Theme.fontFamilyUi
    font.pixelSize: Theme.fontSize("menu")

    contentItem: RowLayout {
        spacing: 6

        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: iconImage.visible ? 16 : 0
            implicitHeight: 16
            visible: iconImage.visible

            Image {
                id: iconImage
                anchors.centerIn: parent
                width: 16
                height: 16
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                visible: String(control.iconSource || "").length > 0
                         || String(control.icon.name || "").length > 0
                source: {
                    var src = String(control.iconSource || "")
                    if (src.length > 0) {
                        return src
                    }
                    var iconName = String(control.icon.name || "")
                    if (iconName.length > 0) {
                        return "image://themeicon/" + iconName
                    }
                    return ""
                }
            }
        }

        Text {
            Layout.preferredWidth: 14
            Layout.alignment: Qt.AlignVCenter
            text: control.checkable && control.checked ? "\u2713" : ""
            color: !control.enabled ? Theme.color("textDisabled")
                  : (control.highlighted ? Theme.color("selectedItemText") : Theme.color("menuText"))
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("menu")
            font.weight: Theme.fontWeight("bold")
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: control.text
            font.family: control.font.family
            font.pixelSize: control.font.pixelSize
            font.weight: control.highlighted ? Theme.fontWeight("medium") : Theme.fontWeight("normal")
            color: !control.enabled ? Theme.color("textDisabled")
                  : (control.highlighted ? Theme.color("selectedItemText") : Theme.color("menuText"))
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            lineHeightMode: Text.ProportionalHeight
            lineHeight: Theme.lineHeight("normal")
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            visible: String(control.secondaryText || "").length > 0
            text: String(control.secondaryText || "")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("small")
            color: !control.enabled ? Theme.color("textDisabled")
                  : (control.highlighted ? Theme.color("selectedItemText") : Theme.color("textSecondary"))
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            visible: String(control.shortcutText || "").length > 0
            implicitWidth: shortcutLabel.implicitWidth + 10
            implicitHeight: Math.max(16, shortcutLabel.implicitHeight + 2)
            radius: Theme.radiusSm
            color: control.highlighted ? Qt.rgba(1, 1, 1, 0.24) : Theme.color("fileManagerControlBg")
            border.width: Theme.borderWidthThin
            border.color: control.highlighted ? Qt.rgba(1, 1, 1, 0.45) : Theme.color("fileManagerControlBorder")

            Text {
                id: shortcutLabel
                anchors.centerIn: parent
                text: String(control.shortcutText || "")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("tiny")
                color: !control.enabled ? Theme.color("textDisabled")
                      : (control.highlighted ? Theme.color("selectedItemText") : Theme.color("textSecondary"))
            }
        }

        Item {
            Layout.preferredWidth: control.hasSubMenu ? 12 : 0
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: 12
            visible: control.hasSubMenu

            Text {
                anchors.centerIn: parent
                text: "\u25B6"
                color: !control.enabled ? Theme.color("textDisabled")
                      : (control.highlighted ? Theme.color("selectedItemText") : Theme.color("menuText"))
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: (control.enabled && control.highlighted) ? Theme.color("menuHover") : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: Math.round(Theme.transitionDuration * 0.6)
                easing.type: Easing.InOutQuad
            }
        }
    }
}
