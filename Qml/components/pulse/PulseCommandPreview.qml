import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as PulseComp

PulseComp.PulseSection {
    id: root

    property string previewTitle: ""
    property string previewImpact: ""

    previewStyle: true

    Label {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        text: root.previewTitle
        color: Theme.color("textPrimary")
        font.pixelSize: Theme.fontSize("body")
        font.weight: Theme.fontWeight("bold")
        elide: Text.ElideRight
    }

    Label {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 28
        text: root.previewImpact
        color: Theme.color("textSecondary")
        font.pixelSize: Theme.fontSize("small")
        elide: Text.ElideRight
    }
}
