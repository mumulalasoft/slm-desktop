import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Slm_Desktop

T.ComboBox {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.7)

    implicitWidth: 200
    implicitHeight: Theme.metric("controlHeightLarge")
    leftPadding: 12
    rightPadding: 34

    delegate: ItemDelegate {
        width: control.width
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            radius: Theme.radiusControl
            color: highlighted ? Theme.color("menuHover") : "transparent"
        }

        contentItem: Text {
            text: parent.text
            font: control.font
            color: parent.highlighted ? Theme.color("selectedItemText") : Theme.color("textPrimary")
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 4
        }
    }

    indicator: Text {
        text: "\u2304"
        color: control.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: Theme.fontSize("menu")
    }

    contentItem: Text {
        leftPadding: 2
        rightPadding: control.indicator.width + 8
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusControlLarge
        color: control.enabled ? Theme.color("controlBg") : Theme.color("controlDisabledBg")
        border.width: control.visualFocus ? Theme.borderWidthThick : Theme.borderWidthThin
        border.color: control.visualFocus ? Theme.color("focusRingStrong")
                                          : (control.enabled ? Theme.color("panelBorder")
                                                             : Theme.color("controlDisabledBorder"))

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: parent.radius + 2
            color: "transparent"
            border.width: 1
            border.color: Theme.color("focusRing")
            visible: control.visualFocus && control.enabled
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 6

        background: PopupSurface {
            popupRadius: Theme.radiusControlLarge
            popupOpacity: Theme.popupSurfaceOpacityStrong
            popupColor: Theme.color("surface")
            popupBorderColor: Theme.color("panelBorder")
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
