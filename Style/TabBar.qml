import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.TabBar {
    id: control
    spacing: Theme.metric("spacingXxs")
    padding: Theme.metric("spacingXxs")
    implicitHeight: Theme.metric("controlHeightLarge") + (Theme.metric("spacingXxs") * 2)

    background: Rectangle {
        radius: Theme.radiusPill
        color: Theme.color("fileManagerControlBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerControlBorder")
    }
}
