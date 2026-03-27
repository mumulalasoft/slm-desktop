import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root
    required property var hostRoot
    required property var dialogRef
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)
    readonly property var propertiesEntry: hostRoot.propertiesEntry
    readonly property var propertiesStat: hostRoot.propertiesStat
    readonly property bool propertiesShowDeviceUsage: !!hostRoot.propertiesShowDeviceUsage
    readonly property var propertiesOpenWithApps: hostRoot.propertiesOpenWithApps
    readonly property int propertiesOpenWithCurrentIndex: Number(hostRoot.propertiesOpenWithCurrentIndex)
    readonly property string propertiesOpenWithName: String(hostRoot.propertiesOpenWithName || "")
    readonly property string propertiesSharePath: String(hostRoot.propertiesSharePath || "")
    readonly property var propertiesShareInfo: (hostRoot.folderShareInfoForPath
                                                && propertiesSharePath.length > 0
                                                && !!(propertiesStat && propertiesStat.isDir))
                                               ? hostRoot.folderShareInfoForPath(propertiesSharePath) : ({ "ok": false, "enabled": false })

    function formatStorageBytes(v) { return hostRoot.formatStorageBytes(v) }
    function fileTypeDisplay(stat, entry) { return hostRoot.fileTypeDisplay(stat, entry) }
    function formatDateTimeHuman(v) { return hostRoot.formatDateTimeHuman(v) }
    function locationDisplay(stat, entry) { return hostRoot.locationDisplay(stat, entry) }
    function applyPropertiesOpenWithSelection(index) { hostRoot.applyPropertiesOpenWithSelection(index) }

ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: 12
    anchors.rightMargin: 12
    anchors.bottomMargin: 12
    anchors.topMargin: 50
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 44
            radius: Theme.radiusMd
            color: Theme.color("fileManagerSearchBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerControlBorder")

            Image {
                anchors.centerIn: parent
                width: 24
                height: 24
                fillMode: Image.PreserveAspectFit
                source: "image://themeicon/" + String(
                            (propertiesEntry
                             && propertiesEntry.iconName) ? propertiesEntry.iconName : "text-x-generic")
                        + "?v=" + root.iconRevision
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            radius: Theme.radiusMd
            color: Theme.color("fileManagerSearchBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerControlBorder")

            Label {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideMiddle
                color: Theme.color("textPrimary")
                text: String(
                          (propertiesEntry
                           && propertiesEntry.name) ? propertiesEntry.name : "")
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        radius: Theme.radiusMdPlus
        color: Theme.color("fileManagerSearchBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerControlBorder")

        RowLayout {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 2

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: hostRoot.propertiesTabIndex === 0 ? Theme.color(
                                                           "selectedItem") : "transparent"

                Label {
                    anchors.centerIn: parent
                    text: "General"
                    color: hostRoot.propertiesTabIndex
                           === 0 ? Theme.color(
                                       "selectedItemText") : Theme.color(
                                       "textPrimary")
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: hostRoot.propertiesTabIndex = 0
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: hostRoot.propertiesTabIndex === 1 ? Theme.color(
                                                           "selectedItem") : "transparent"

                Label {
                    anchors.centerIn: parent
                    text: "Permissions"
                    color: hostRoot.propertiesTabIndex
                           === 1 ? Theme.color(
                                       "selectedItemText") : Theme.color(
                                       "textPrimary")
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: hostRoot.propertiesTabIndex = 1
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: hostRoot.propertiesTabIndex === 2 ? Theme.color(
                                                           "selectedItem") : "transparent"
                enabled: !!(propertiesStat && propertiesStat.isDir)
                opacity: enabled ? 1.0 : 0.5

                Label {
                    anchors.centerIn: parent
                    text: "Sharing"
                    color: hostRoot.propertiesTabIndex
                           === 2 ? Theme.color(
                                       "selectedItemText") : Theme.color(
                                       "textPrimary")
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: parent.enabled
                    onClicked: hostRoot.propertiesTabIndex = 2
                }
            }
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: hostRoot.propertiesTabIndex

        Item {

            ColumnLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 8

                Label {
                    text: "Info"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("medium")
                }

                RowLayout {
                    Label {
                        text: "Size:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: {
                            var isDir = !!(propertiesStat
                                           && propertiesStat.isDir)
                            var sz = Number(
                                        (propertiesStat
                                         && propertiesStat.size) ? propertiesStat.size : 0)
                            return isDir ? "-" : formatStorageBytes(sz)
                        }
                        color: Theme.color("textPrimary")
                    }
                }

                RowLayout {

                    Label {
                        visible: !(propertiesStat
                                   && propertiesStat.isDir)
                        text: "Type:"
                        color: Theme.color("textSecondary")

                        // Layout.preferredWidth: 84
                    }
                    Label {
                        visible: !(propertiesStat
                                   && propertiesStat.isDir)
                        text: fileTypeDisplay(propertiesStat,
                                              propertiesEntry)
                        color: Theme.color("textPrimary")
                    }
                }

                RowLayout {
                    Label {
                        text: "Created:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: formatDateTimeHuman(
                                  (propertiesStat
                                   && propertiesStat.created) ? propertiesStat.created : "")
                        color: Theme.color("textPrimary")
                    }
                }

                RowLayout {
                    Label {
                        text: "Modified:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: formatDateTimeHuman(
                                  (propertiesStat
                                   && propertiesStat.modified) ? propertiesStat.modified : ((propertiesStat && propertiesStat.lastModified) ? propertiesStat.lastModified : ""))
                        color: Theme.color("textPrimary")
                    }
                }
                RowLayout {
                    Label {
                        text: "Media type:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: String(
                                  (propertiesStat
                                   && propertiesStat.mimeType) ? propertiesStat.mimeType : "-")
                        color: Theme.color("textPrimary")
                    }
                }
                RowLayout {
                    Label {
                        text: "Resolution:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: {
                            var w = Number(
                                        (propertiesStat
                                         && propertiesStat.imageWidth) ? propertiesStat.imageWidth : 0)
                            var h = Number(
                                        (propertiesStat
                                         && propertiesStat.imageHeight) ? propertiesStat.imageHeight : 0)
                            return (w > 0
                                    && h > 0) ? (String(
                                                     w) + " × " + String(
                                                     h) + " px") : "-"
                        }
                        color: Theme.color("textPrimary")
                    }
                }
                RowLayout {

                    Label {
                        text: "Location:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: locationDisplay(propertiesStat,
                                              propertiesEntry)
                        color: Theme.color("accent")
                        elide: Text.ElideMiddle
                    }
                }

                RowLayout {

                    Label {
                        visible: !(propertiesStat
                                   && propertiesStat.isDir)
                        text: "Open with:"
                        color: Theme.color("textSecondary")
                    }
                    ComboBox {
                        id: propertiesOpenWithCombo
                        visible: !(propertiesStat
                                   && propertiesStat.isDir)
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        model: hostRoot.propertiesOpenWithApps
                        textRole: "name"
                        enabled: (hostRoot.propertiesOpenWithApps
                                  && hostRoot.propertiesOpenWithApps.length > 0)
                        currentIndex: hostRoot.propertiesOpenWithCurrentIndex
                        onActivated: function (index) {
                            hostRoot.applyPropertiesOpenWithSelection(index)
                        }
                        delegate: ItemDelegate {
                            width: propertiesOpenWithCombo.width
                            highlighted: propertiesOpenWithCombo.highlightedIndex === index
                            contentItem: RowLayout {
                                spacing: 8
                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    fillMode: Image.PreserveAspectFit
                                    source: "image://themeicon/" + String(
                                                (modelData
                                                 && modelData.iconName) ? modelData.iconName : "application-x-executable-symbolic")
                                            + "?v=" + root.iconRevision
                                }
                                Label {
                                    Layout.fillWidth: true
                                    color: Theme.color("textPrimary")
                                    elide: Text.ElideRight
                                    text: {
                                        var base = String(
                                                    (modelData
                                                     && modelData.name) ? modelData.name : "")
                                        return !!(modelData
                                                  && modelData.defaultApp) ? ("Default: " + base) : base
                                    }
                                }
                            }
                            background: Rectangle {
                                color: highlighted ? Theme.color(
                                                         "selectedItem") : "transparent"
                            }
                        }
                        contentItem: RowLayout {
                            spacing: 8
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 26
                            Image {
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                fillMode: Image.PreserveAspectFit
                                source: "image://themeicon/" + String(
                                            (hostRoot.propertiesOpenWithApps
                                             && hostRoot.propertiesOpenWithCurrentIndex >= 0
                                             && hostRoot.propertiesOpenWithCurrentIndex
                                             < hostRoot.propertiesOpenWithApps.length
                                             && hostRoot.propertiesOpenWithApps[hostRoot.propertiesOpenWithCurrentIndex] && hostRoot.propertiesOpenWithApps[hostRoot.propertiesOpenWithCurrentIndex].iconName) ? hostRoot.propertiesOpenWithApps[hostRoot.propertiesOpenWithCurrentIndex].iconName : "application-x-executable-symbolic")
                                        + "?v=" + root.iconRevision
                            }
                            Label {
                                Layout.fillWidth: true
                                color: Theme.color("textPrimary")
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                                text: hostRoot.propertiesOpenWithName.length
                                      > 0 ? hostRoot.propertiesOpenWithName : "Default application"
                            }
                        }
                        indicator: Rectangle {
                            x: propertiesOpenWithCombo.width - width - 6
                            y: Math.round(
                                   (propertiesOpenWithCombo.height - height) * 0.5)
                            width: 22
                            height: 22
                            radius: Theme.radiusMd
                            color: Theme.color("selectedItem")
                            Label {
                                anchors.centerIn: parent
                                text: "▾"
                                color: Theme.color("selectedItemText")
                            }
                        }
                        background: Rectangle {
                            radius: Theme.radiusMdPlus
                            color: Theme.color("fileManagerSearchBg")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color(
                                              "fileManagerControlBorder")
                        }
                    }
                }

                Rectangle {
                    visible: hostRoot.propertiesShowDeviceUsage
                    Layout.fillWidth: true
                    Layout.preferredHeight: 88
                    radius: Theme.radiusControl
                    color: "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Label {
                            text: "Device Usage"
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("body")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 8
                            radius: Theme.radiusSm
                            color: Theme.color(
                                       "fileManagerControlBorder")
                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                height: parent.height
                                radius: Theme.radiusSm
                                width: {
                                    var total = Number(
                                                (propertiesStat
                                                 && propertiesStat.volumeBytesTotal) ? propertiesStat.volumeBytesTotal : -1)
                                    var avail = Number(
                                                (propertiesStat
                                                 && propertiesStat.volumeBytesAvailable) ? propertiesStat.volumeBytesAvailable : -1)
                                    if (!(total > 0) || !(avail >= 0)) {
                                        return 0
                                    }
                                    var usedRatio = Math.max(
                                                0, Math.min(
                                                    1,
                                                    1.0 - (avail / total)))
                                    return Math.round(
                                                parent.width * usedRatio)
                                }
                                color: {
                                    var total = Number(
                                                (propertiesStat
                                                 && propertiesStat.volumeBytesTotal) ? propertiesStat.volumeBytesTotal : -1)
                                    var avail = Number(
                                                (propertiesStat
                                                 && propertiesStat.volumeBytesAvailable) ? propertiesStat.volumeBytesAvailable : -1)
                                    if (!(total > 0) || !(avail >= 0)) {
                                        return Theme.color(
                                                    "storageUsageUnknown")
                                    }
                                    var usedRatio = Math.max(
                                                0, Math.min(
                                                    1,
                                                    1.0 - (avail / total)))
                                    if (usedRatio >= 0.9) {
                                        return Theme.color(
                                                    "storageUsageCritical")
                                    }
                                    if (usedRatio >= 0.75) {
                                        return Theme.color(
                                                    "storageUsageWarn")
                                    }
                                    return Theme.color(
                                                "storageUsageInfo")
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.color("textPrimary")
                            text: formatStorageBytes(
                                      Number(
                                          (propertiesStat
                                           && propertiesStat.volumeBytesAvailable) ? propertiesStat.volumeBytesAvailable : 0))
                                  + " free out of " + formatStorageBytes(
                                      Number(
                                          (propertiesStat
                                           && propertiesStat.volumeBytesTotal) ? propertiesStat.volumeBytesTotal : 0))
                        }
                    }
                }
            }
        }

        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                GridLayout {
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 10
                    Label {
                        text: "Owner:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: String(
                                  (propertiesStat
                                   && propertiesStat.owner) ? propertiesStat.owner : "-")
                        color: Theme.color("textPrimary")
                    }
                    Label {
                        text: "Group:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: String(
                                  (propertiesStat
                                   && propertiesStat.group) ? propertiesStat.group : "-")
                        color: Theme.color("textPrimary")
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 128
                    radius: Theme.radiusControl
                    color: Theme.color("fileManagerSearchBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color(
                                      "fileManagerControlBorder")

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                Layout.preferredWidth: 72
                                text: "Owner"
                                color: Theme.color("textPrimary")
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permOwnerRead) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "◉"
                                    color: (propertiesStat
                                            && propertiesStat.permOwnerRead) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permOwnerWrite) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "✎"
                                    color: (propertiesStat
                                            && propertiesStat.permOwnerWrite) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permOwnerExec) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "▶"
                                    color: (propertiesStat
                                            && propertiesStat.permOwnerExec) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.color("panelBorder")
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                Layout.preferredWidth: 72
                                text: "Group"
                                color: Theme.color("textPrimary")
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permGroupRead) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "◉"
                                    color: (propertiesStat
                                            && propertiesStat.permGroupRead) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permGroupWrite) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "✎"
                                    color: (propertiesStat
                                            && propertiesStat.permGroupWrite) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permGroupExec) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "▶"
                                    color: (propertiesStat
                                            && propertiesStat.permGroupExec) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.color("panelBorder")
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                Layout.preferredWidth: 72
                                text: "Everyone"
                                color: Theme.color("textPrimary")
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permOtherRead) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "◉"
                                    color: (propertiesStat
                                            && propertiesStat.permOtherRead) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permOtherWrite) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "✎"
                                    color: (propertiesStat
                                            && propertiesStat.permOtherWrite) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                radius: Theme.radiusMd
                                color: (propertiesStat
                                        && propertiesStat.permOtherExec) ? Theme.color("selectedItem") : Theme.color("fileManagerControlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color(
                                                  "fileManagerControlBorder")
                                Label {
                                    anchors.centerIn: parent
                                    text: "▶"
                                    color: (propertiesStat
                                            && propertiesStat.permOtherExec) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: "-" + String(
                                  (propertiesStat
                                   && propertiesStat.permissionsSymbolic) ? propertiesStat.permissionsSymbolic : "---------")
                        color: Theme.color("textSecondary")
                    }
                    Rectangle {
                        radius: Theme.radiusMd
                        color: Theme.color("fileManagerControlBg")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color(
                                          "fileManagerControlBorder")
                        Layout.preferredHeight: 28
                        Layout.preferredWidth: 56
                        Label {
                            anchors.centerIn: parent
                            text: String(
                                      (propertiesStat
                                       && propertiesStat.permissionsOctal) ? propertiesStat.permissionsOctal : "---")
                            color: Theme.color("textPrimary")
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Item {
            ColumnLayout {
                spacing: 8

                Label {
                    text: "Sharing"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("medium")
                }

                Label {
                    Layout.fillWidth: true
                    visible: !!(propertiesShareInfo && propertiesShareInfo.enabled)
                    text: "Dibagikan di jaringan"
                    color: Theme.color("accent")
                }

                Label {
                    Layout.fillWidth: true
                    visible: !propertiesShareInfo.enabled
                    text: "Belum dibagikan"
                    color: Theme.color("textSecondary")
                }

                RowLayout {
                    visible: !!(propertiesShareInfo && propertiesShareInfo.enabled)
                    Label {
                        text: "Nama:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: String(propertiesShareInfo.shareName || "-")
                        color: Theme.color("textPrimary")
                    }
                }

                RowLayout {
                    visible: !!(propertiesShareInfo && propertiesShareInfo.enabled)
                    Label {
                        text: "Akses:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: {
                            var mode = String(propertiesShareInfo.access || "owner")
                            if (mode === "anyone" || mode === "all")
                                return "Siapa pun di jaringan ini"
                            if (mode === "users" || mode === "specific")
                                return "Pengguna tertentu"
                            return "Hanya saya"
                        }
                        color: Theme.color("textPrimary")
                    }
                }

                RowLayout {
                    visible: !!(propertiesShareInfo && propertiesShareInfo.enabled)
                    Label {
                        text: "Izin:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: String(propertiesShareInfo.permission || "read") === "write"
                              ? "Bisa mengubah file" : "Hanya lihat"
                        color: Theme.color("textPrimary")
                    }
                }

                RowLayout {
                    visible: !!(propertiesShareInfo && propertiesShareInfo.enabled)
                    Label {
                        text: "Status backend:"
                        color: Theme.color("textSecondary")
                    }
                    Label {
                        text: propertiesShareInfo.backendApplied ? "Siap"
                              : (propertiesShareInfo.backendPending ? "Perlu tindakan" : "Belum siap")
                        color: propertiesShareInfo.backendApplied
                               ? Theme.color("success")
                               : Theme.color("warning")
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: !!(propertiesShareInfo && propertiesShareInfo.enabled
                                && propertiesShareInfo.backendError)
                    text: String(propertiesShareInfo.backendMessage
                                 || propertiesShareInfo.backendError || "")
                    color: Theme.color("warning")
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    spacing: 8
                    Button {
                        text: propertiesShareInfo.enabled ? "Ubah" : "Bagikan Folder..."
                        enabled: !!(propertiesStat && propertiesStat.isDir)
                        onClicked: hostRoot.openFolderShareDialog(propertiesSharePath)
                    }
                    Button {
                        text: "Salin alamat"
                        visible: !!(propertiesShareInfo && propertiesShareInfo.enabled)
                        onClicked: hostRoot.copyFolderShareAddress(propertiesSharePath)
                    }
                    Button {
                        text: "Hentikan berbagi"
                        visible: !!(propertiesShareInfo && propertiesShareInfo.enabled)
                        onClicked: hostRoot.disableFolderShare(propertiesSharePath)
                    }
                    Button {
                        text: "Periksa backend"
                        visible: !!(propertiesStat && propertiesStat.isDir)
                        onClicked: {
                            var status = hostRoot.folderSharingEnvironment()
                            hostRoot.notifyResult("Bagikan Folder", status)
                        }
                    }
                    Button {
                        text: "Perbaiki backend"
                        visible: !!(propertiesStat && propertiesStat.isDir)
                        onClicked: {
                            var fixed = hostRoot.repairFolderSharingEnvironment()
                            hostRoot.notifyResult("Bagikan Folder", fixed)
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Item {
            Layout.fillWidth: true
        }
        Button {
            text: "Close"
            onClicked: dialogRef.close()
        }
    }
}

}
