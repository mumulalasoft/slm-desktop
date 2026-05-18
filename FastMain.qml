import QtQuick 2.15
import QtQuick.Window 2.15
import SlmStyle as DSStyle

Window {
    id: root
    property var fullShellComponent: null
    property var fullShellWindow: null

    function finishFullShellLoad() {
        if (!fullShellComponent || fullShellComponent.status !== Component.Ready) {
            return
        }
        fullShellWindow = fullShellComponent.createObject(null)
        if (!fullShellWindow) {
            console.error("SLM fast startup: failed to create Main.qml")
            return
        }
        if (fullShellWindow.show) {
            fullShellWindow.show()
        }
        Qt.callLater(function() {
            root.close()
        })
    }

    function startFullShellLoad() {
        fullShellComponent = Qt.createComponent(Qt.resolvedUrl("Main.qml"), Component.Asynchronous)
        if (fullShellComponent.status === Component.Ready) {
            finishFullShellLoad()
            return
        }
        if (fullShellComponent.status === Component.Error) {
            console.error("SLM fast startup:", fullShellComponent.errorString())
            return
        }
        fullShellComponent.statusChanged.connect(function() {
            if (fullShellComponent.status === Component.Ready) {
                finishFullShellLoad()
            } else if (fullShellComponent.status === Component.Error) {
                console.error("SLM fast startup:", fullShellComponent.errorString())
            }
        })
    }

    visible: true
    visibility: Window.FullScreen
    flags: Qt.FramelessWindowHint
    title: "SLM Desktop"
    color: DSStyle.Theme.color("windowBg")

    Image {
        anchors.fill: parent
        source: "qrc:/images/wallpaper.jpeg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        color: DSStyle.Theme.color("wallpaperTint")
    }

    Component.onCompleted: Qt.callLater(root.startFullShellLoad)
}
