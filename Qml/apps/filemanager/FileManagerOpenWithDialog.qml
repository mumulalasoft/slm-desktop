import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

AppDialog {
    id: root

    required property var hostRoot
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)

    title: hostRoot.openWithDialogMode === "setdefault"
           ? "Set Default Application"
           : "Open With Application"
    standardButtons: Dialog.Cancel

    readonly property int rowHeight: 34
    readonly property int minVisibleRows: 2
    readonly property int maxVisibleRows: 8
    readonly property int visibleRows: Math.max(minVisibleRows,
                                                Math.min(maxVisibleRows, hostRoot.filteredOpenWithCount()))

    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)
    width: 520
    height: 116 + (visibleRows * rowHeight)

    contentItem: ColumnLayout {
        spacing: 8

        DSStyle.TextField {
            id: openWithSearchField
            Layout.fillWidth: true
            placeholderText: "Search application"
            text: hostRoot.openWithSearchText
            onTextChanged: hostRoot.openWithSearchText = text
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (root.visibleRows * root.rowHeight) + 12
            color: Theme.color("windowCard")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("windowCardBorder")
            radius: Theme.radiusControl

            ListView {
                id: openWithList
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                model: hostRoot.contextOpenWithAllApps
                spacing: 1

                delegate: Item {
                    required property var modelData

                    readonly property string appId: String((modelData && modelData.id) ? modelData.id : "")
                    readonly property string appName: String((modelData && modelData.name) ? modelData.name : appId)
                    readonly property string iconName: String((modelData && modelData.iconName) ? modelData.iconName : "")
                    readonly property bool matchesFilter: {
                        var q = String(hostRoot.openWithSearchText || "").trim().toLowerCase()
                        if (q.length === 0) {
                            return true
                        }
                        return appName.toLowerCase().indexOf(q) >= 0 || appId.toLowerCase().indexOf(q) >= 0
                    }

                    visible: matchesFilter
                    width: openWithList.width
                    height: matchesFilter ? 34 : 0

                    Rectangle {
                        anchors.fill: parent
                        color: appMouse.containsMouse ? Theme.color("hoverItem") : "transparent"
                        radius: Theme.radiusMd

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            Image {
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                source: "image://themeicon/" + (iconName.length > 0 ? iconName : "application-x-executable-symbolic")
                                        + "?v=" + root.iconRevision
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: appName
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("smallPlus")
                                elide: Text.ElideRight
                            }

                            Text {
                                readonly property string tagText: hostRoot.openWithSecondaryTag(modelData)
                                text: tagText
                                visible: tagText.length > 0
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("tiny")
                            }
                        }
                    }

                    MouseArea {
                        id: appMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (appId.length === 0) {
                                return
                            }
                            if (hostRoot.openWithDialogMode === "setdefault") {
                                hostRoot.setDefaultContextEntryApp(appId)
                            } else {
                                hostRoot.openContextEntryInApp(appId)
                            }
                            root.close()
                        }
                    }
                }
            }
        }
    }

    onOpened: {
        openWithSearchField.forceActiveFocus()
        openWithSearchField.selectAll()
    }
}
