import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Rectangle {
    id: root

    required property int index
    required property string name
    required property string path
    required property bool isDir
    required property string suffix
    required property string lastModified
    required property real bytes
    required property string mimeType

    property var selectedPaths: ({})
    property bool allowMultiple: false
    property bool selectFolders: false
    property string chooserMode: "open"
    property real listWidth: 0
    property real nameColumnWidth: 220
    property real dateColumnWidth: 140
    property real kindColumnWidth: 92
    property int iconRevision: 0

    signal rowClicked(int index, int modifiers)
    signal rowDoubleClicked(int index)

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    function formatBytes(bytesValue) {
        var b = Number(bytesValue)
        if (!isFinite(b) || b < 0) {
            return ""
        }
        if (b < 1024) return Math.floor(b) + " B"
        var units = ["KB", "MB", "GB", "TB"]
        var v = b / 1024.0
        var idx = 0
        while (v >= 1024.0 && idx < units.length - 1) {
            v /= 1024.0
            idx += 1
        }
        return v.toFixed(v < 10 ? 1 : 0) + " " + units[idx]
    }

    readonly property bool selected: selectedPaths[path] === true

    width: listWidth
    height: 32
    radius: 0
    color: selected
           ? Theme.color("accent")
           : (rowMouse.containsMouse ? Theme.color("accentSoft") : "transparent")
    Behavior on color {
        enabled: root.microAnimationAllowed()
        ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Row {
            width: root.nameColumnWidth
            height: parent.height
            spacing: 8

            Image {
                width: 18
                height: 18
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                source: "image://themeicon/" + (root.isDir ? "folder" : "text-x-generic") + "?v=" + root.iconRevision
            }

            Label {
                width: parent.width - 26
                height: parent.height
                text: root.name
                color: root.selected ? Theme.color("accentText") : Theme.color("textPrimary")
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }

        Label {
            width: Math.max(90, Number(root.dateColumnWidth || 140))
            height: parent.height
            text: root.lastModified.length > 0 ? root.lastModified : "-"
            color: root.selected ? Theme.color("accentText") : Theme.color("textSecondary")
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Item {
            width: Math.max(80, Number(root.kindColumnWidth || 92))
            height: parent.height

            readonly property string kindText: {
                if (root.isDir) {
                    return "Folder"
                }
                var kind = root.suffix.length > 0 ? root.suffix.toUpperCase() : "File"
                var sz = root.formatBytes(root.bytes)
                return sz.length > 0 ? (kind + " • " + sz) : kind
            }
            readonly property string kindTooltip: {
                var t = kindText
                var mt = String(root.mimeType || "").trim()
                if (mt.length > 0) {
                    t += "\n" + mt
                }
                return t
            }

            Label {
                anchors.fill: parent
                text: parent.kindText
                color: root.selected ? Theme.color("accentText") : Theme.color("textSecondary")
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: kindHover
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
            ToolTip.visible: kindHover.containsMouse && parent.kindTooltip.length > 0
            ToolTip.delay: 250
            ToolTip.timeout: 3000
            ToolTip.text: parent.kindTooltip
        }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: function(mouse) {
            root.rowClicked(root.index, mouse ? mouse.modifiers : 0)
        }
        onDoubleClicked: root.rowDoubleClicked(root.index)
    }
}
