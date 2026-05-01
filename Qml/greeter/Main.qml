import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root
    visible: true
    title: "SLM Desktop"
    color: "black"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    Item {
        id: sceneRoot
        anchors.fill: parent

        LoginPage {
            id: loginPage
            anchors.fill: parent
        }
    }
}
