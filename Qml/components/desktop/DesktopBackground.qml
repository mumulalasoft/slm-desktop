import QtQuick 2.15
import Slm_Desktop

Rectangle {
    id: root

    property string imageSource: ""

    color: Theme.color("windowBg")
    Behavior on color {
        ColorAnimation {
            duration: Theme.transitionDuration
            easing.type: Easing.InOutQuad
        }
    }

    Image {
        anchors.fill: parent
        source: root.imageSource
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
    }

    Rectangle {
        id: tintLayer
        anchors.fill: parent
        color: Theme.color("wallpaperTint")
        Behavior on color {
            ColorAnimation {
                duration: Theme.transitionDuration
                easing.type: Easing.InOutQuad
            }
        }
    }
}
