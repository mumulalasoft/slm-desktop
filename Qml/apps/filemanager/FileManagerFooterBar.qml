import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle
import SlmStyle as DSStyle

Rectangle {
    id: root

    required property var hostRoot

    function currentSelectedShareInfo() {
        if (!hostRoot || !hostRoot.selectedEntryIndexes
                || hostRoot.selectedEntryIndexes.length !== 1
                || !hostRoot.selectedEntry) {
            return ({
                        "ok": false,
                        "enabled": false
                    })
        }

        var entry = hostRoot.selectedEntry()
        if (!entry || !entry.ok || !entry.isDir) {
            return ({
                        "ok": false,
                        "enabled": false
                    })
        }

        var path = String(entry.path || "")
        if (path.length <= 0) {
            return ({
                        "ok": false,
                        "enabled": false
                    })
        }

        if (hostRoot.cachedFolderShareInfoForPath) {
            return hostRoot.cachedFolderShareInfoForPath(path)
        }
        if (hostRoot.folderShareInfoForPath) {
            return hostRoot.folderShareInfoForPath(path)
        }
        return ({
                    "ok": false,
                    "enabled": false
                })
    }

    readonly property var selectedShareInfo: currentSelectedShareInfo()
    readonly property bool selectedFolderShared: !!(selectedShareInfo
                                                    && selectedShareInfo.ok
                                                    && selectedShareInfo.enabled)
    readonly property string selectedShareAddress: String(
                                                       selectedShareInfo
                                                       && selectedShareInfo.address
                                                       ? selectedShareInfo.address : "")

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.07)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.metric("spacingMd")
        anchors.rightMargin: Theme.metric("spacingMd")
        spacing: Theme.metric("spacingSm")

        DSStyle.Label {
            Layout.fillWidth: true
            text: {
                var items = Number(hostRoot.fileModel
                                   ? Number(hostRoot.fileModel.count || 0) : 0)
                var selected = Number(
                            hostRoot.selectedEntryIndexes
                            ? hostRoot.selectedEntryIndexes.length : 0)
                if (selected > 0) {
                    return String(selected) + " of " + String(items) + " selected"
                }
                return String(items) + (items === 1 ? " item" : " items")
            }
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideMiddle
        }

        Rectangle {
            id: sharedStatusPill
            visible: root.selectedFolderShared
            Layout.preferredWidth: sharedStatusLabel.implicitWidth
                                   + Theme.metric("spacingMd") * 2
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignVCenter
            radius: height / 2
            color: Theme.darkMode ? Qt.rgba(0.04, 0.32, 0.17, 1)
                                  : Qt.rgba(0.88, 0.97, 0.91, 1)
            border.width: Theme.borderWidthHairline
            border.color: Theme.darkMode ? Qt.rgba(0.30, 0.78, 0.49, 0.52)
                                         : Qt.rgba(0.10, 0.52, 0.25, 0.28)

            DSStyle.Label {
                id: sharedStatusLabel
                anchors.centerIn: parent
                text: qsTr("Shared")
                color: Theme.darkMode ? Qt.rgba(0.70, 0.95, 0.78, 1)
                                      : Qt.rgba(0.05, 0.38, 0.18, 1)
                font.pixelSize: Theme.fontSize("small")
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: sharedStatusHover
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                hoverEnabled: true
            }

            ToolTip.visible: sharedStatusHover.containsMouse
                             && root.selectedShareAddress.length > 0
            ToolTip.delay: 450
            ToolTip.text: root.selectedShareAddress
        }
    }
}
