import QtQuick 2.15
import Slm_Desktop

Rectangle {
    id: root
    property bool compactWindow: false

    radius: compactWindow ? 0 : Theme.radiusXxxl
    color: Theme.color("pulsePanelBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("pulsePanelBorder")
}
