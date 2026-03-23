import QtQuick
import Style as DSStyle
import Slm_Desktop

DSStyle.Label {
    id: control

    property bool emphasized: false

    color: Theme.color("textSecondary")
    font.family: Theme.fontFamilyUi
    font.pixelSize: Theme.fontSize("body")
    font.weight: emphasized ? Theme.fontWeight("semibold") : Theme.fontWeight("normal")
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
}
