import QtQuick 2.15
import Slm_Desktop

Rectangle {
    id: root

    property string imageSource: ""

    color: Theme.color("windowBg")
    Behavior on color {
        ColorAnimation {
            duration: Theme.transitionDuration
            easing.type: Theme.easingStandard
        }
    }

    Image {
        anchors.fill: parent
        source: root.imageSource
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
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
                easing.type: Theme.easingStandard
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.darkMode ? Qt.rgba(0, 0, 0, 0.18) : Qt.rgba(1, 1, 1, 0.10)
            }
            GradientStop {
                position: 0.54
                color: Qt.rgba(0, 0, 0, 0.0)
            }
            GradientStop {
                position: 1.0
                color: Theme.darkMode ? Qt.rgba(0, 0, 0, 0.32) : Qt.rgba(0, 0, 0, 0.16)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: Theme.borderWidthNone
        opacity: Theme.darkMode ? Theme.opacityFaint : Theme.opacitySubtle
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.22 : 0.10)
            }
            GradientStop {
                position: 0.18
                color: Qt.rgba(0, 0, 0, 0.0)
            }
            GradientStop {
                position: 0.82
                color: Qt.rgba(0, 0, 0, 0.0)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.22 : 0.10)
            }
        }
    }
}
