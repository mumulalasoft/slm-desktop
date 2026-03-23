import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: statusBar
    property int itemCount: 0
    property int selectedCount: 0
    property bool indexing: false
    property string indexingPath: ""
    property int indexingCurrent: 0
    property int indexingTotal: 0
    property int contentScalePercent: 100
    readonly property var scaleSteps: [75, 100, 150, 200, 300, 400]
    signal contentScalePercentRequested(int value)

    function scaleIndexForValue(value) {
        var v = Number(value || 100)
        var bestIdx = 0
        var bestDist = Math.abs(scaleSteps[0] - v)
        for (var i = 1; i < scaleSteps.length; ++i) {
            var d = Math.abs(scaleSteps[i] - v)
            if (d < bestDist) {
                bestDist = d
                bestIdx = i
            }
        }
        return bestIdx
    }

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 10

        Label {
            text: statusBar.indexing
                  ? ("Indexing: " + String(statusBar.indexingCurrent)
                     + (statusBar.indexingTotal > 0 ? ("/" + String(statusBar.indexingTotal)) : "")
                     + "  " + (statusBar.indexingPath.length > 0 ? statusBar.indexingPath : ""))
                  : (String(statusBar.itemCount) + " items"
                     + (statusBar.selectedCount > 0
                        ? ("  •  " + String(statusBar.selectedCount) + " selected")
                        : ""))
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        Label {
            text: "Scale"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
        }

        Slider {
            id: scaleSlider
            Layout.preferredWidth: 170
            from: 0
            to: Math.max(0, statusBar.scaleSteps.length - 1)
            stepSize: 1
            snapMode: Slider.SnapAlways
            live: true
            value: statusBar.scaleIndexForValue(statusBar.contentScalePercent)
            onMoved: {
                var idx = Math.max(0, Math.min(statusBar.scaleSteps.length - 1, Math.round(value)))
                statusBar.contentScalePercentRequested(statusBar.scaleSteps[idx])
            }
            onValueChanged: {
                if (!pressed) {
                    var idx = Math.max(0, Math.min(statusBar.scaleSteps.length - 1, Math.round(value)))
                    var target = statusBar.scaleSteps[idx]
                    if (target !== statusBar.contentScalePercent) {
                        statusBar.contentScalePercentRequested(target)
                    }
                }
            }
        }

        Label {
            text: String(statusBar.contentScalePercent) + "%"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 42
        }

        Rectangle {
            width: 140
            height: 8
            radius: Theme.radiusSm
            color: Theme.color("panelBorder")
            visible: statusBar.indexing
            Rectangle {
                width: statusBar.indexingTotal > 0
                       ? Math.max(6, Math.round(parent.width * Math.min(1, statusBar.indexingCurrent / Math.max(1, statusBar.indexingTotal))))
                       : Math.max(6, Math.round(parent.width * 0.25))
                height: parent.height
                radius: parent.radius
                color: Theme.color("textPrimary")
            }
        }
    }
}
