import QtQuick
import QtQuick.Layouts
import Style as DSStyle
import Slm_Desktop

Item {
    id: control

    property string text: ""
    property bool emphasized: false
    property int rowSpacing: Theme.metric("spacingMd")
    default property alias trailingData: trailingHost.data

    implicitWidth: row.implicitWidth
    implicitHeight: Math.max(Theme.metric("controlHeightRegular"), row.implicitHeight)

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: control.rowSpacing

        DSStyle.IndicatorSectionLabel {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: control.text
            emphasized: control.emphasized
        }

        Item {
            id: trailingHost
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
