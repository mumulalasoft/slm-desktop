import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Window {
    id: root
    visible: true
    visibility: Window.FullScreen
    flags: Qt.FramelessWindowHint | Qt.Window
    title: "SLM Desktop"
    opacity: 0

    NumberAnimation on opacity {
        id: fadeIn
        from: 0; to: 1
        duration: 600
        easing.type: Easing.OutCubic
        running: false
    }

    Component.onCompleted: fadeIn.start()

    LoginPage {
        id: loginPage
        anchors.fill: parent
    }
}
