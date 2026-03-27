import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

Rectangle {
    id: root

    required property var hostRoot
    property var contentViewRef: null
    readonly property var searchFieldRef: toolbarSearchField
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)

    function focusSearchField(selectAllText) {
        hostRoot.toolbarSearchExpanded = true
        Qt.callLater(function() {
            toolbarSearchField.forceActiveFocus()
            if (!!selectAllText) {
                toolbarSearchField.selectAll()
            }
        })
    }

    function clearSearchField() {
        toolbarSearchField.text = ""
    }

    function blurSearchField() {
        toolbarSearchField.focus = false
    }

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 12
        spacing: 8

        Rectangle {
            Layout.preferredWidth: 76
            Layout.preferredHeight: 26
            radius: Theme.radiusWindowAlt
            color: Theme.color("fileManagerControlBg")
            border.width: Theme.borderWidthNone
            border.color: Theme.color("fileManagerControlBorder")

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 3
                anchors.rightMargin: 3
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusCard
                    color: prevMouse.containsMouse ? Theme.color("fileManagerTabActive") : "transparent"
                    Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: true
                        source: "image://themeicon/go-previous-symbolic?v=" + root.iconRevision
                    }
                    MouseArea {
                        id: prevMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: hostRoot.canNavigateBack
                        onClicked: hostRoot.navigateBack()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusCard
                    color: nextMouse.containsMouse ? Theme.color("fileManagerTabActive") : "transparent"
                    Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: true
                        source: "image://themeicon/go-next-symbolic?v=" + root.iconRevision
                        opacity: nextMouse.enabled ? 1.0 : 0.5
                    }
                    MouseArea {
                        id: nextMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: hostRoot.canNavigateForward
                        onClicked: hostRoot.navigateForward()
                    }
                }
            }
        }

        Label {
            Layout.preferredWidth: 300
            Layout.maximumWidth: 460
            text: {
                var p = "~"
                var columnsMode = hostRoot.viewMode === "columns"
                if (columnsMode
                        && contentViewRef && String(contentViewRef.columnsDisplayPath || "").length > 0) {
                    p = String(contentViewRef.columnsDisplayPath || "~")
                } else if (hostRoot.fileModel && hostRoot.fileModel.currentPath) {
                    p = String(hostRoot.fileModel.currentPath)
                }
                if (!columnsMode) {
                    return hostRoot.tabTitleFromPath(p)
                }
                if (p === "__recent__") {
                    return "Recent"
                }
                return p
            }
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("hero")
            font.family: Theme.fontFamilyDisplay
            font.weight: Theme.fontWeight("semibold")
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Item {
            Layout.fillWidth: true
        }

        RowLayout {
            visible: !hostRoot.trashView
            spacing: 8

            Rectangle {
                width: 138
                height: 26
                radius: Theme.radiusWindowAlt
                color: Theme.color("fileManagerControlBg")
                border.width: Theme.borderWidthNone
                border.color: Theme.color("fileManagerControlBorder")
                Row {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3

                    Repeater {
                        model: [{
                                "iconName": "view-grid-symbolic",
                                "mode": "grid"
                            }, {
                                "iconName": "view-list-symbolic",
                                "mode": "list"
                            }, {
                                "iconName": "view-column-symbolic",
                                "mode": "columns"
                            }]

                        delegate: Rectangle {
                            required property var modelData
                            width: Math.floor((parent.width - 6) / 3)
                            height: parent.height
                            radius: Theme.radiusLg
                            color: hostRoot.viewMode === String(
                                       modelData.mode) ? Theme.color("fileManagerTabActive") : "transparent"
                            border.width: Theme.borderWidthNone
                            border.color: Theme.color("fileManagerControlBorder")

                            Image {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: true
                                source: "image://themeicon/" + String(
                                            modelData.iconName
                                            || "view-grid-symbolic")
                                        + "?v=" + root.iconRevision
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                enabled: true
                                onClicked: {
                                    var nextMode = String(parent.modelData.mode
                                                          || "list")
                                    if (nextMode === "grid" || nextMode === "list"
                                            || nextMode === "columns") {
                                        hostRoot.viewMode = nextMode
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                id: toolbarSearch
                width: 40
                height: 30

                Rectangle {
                    id: toolbarSearchPanel
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: hostRoot.toolbarSearchExpanded ? 220 : 40
                    height: 26
                    radius: Theme.radiusWindowAlt
                    color: Theme.color("fileManagerControlBg")
                    border.width: Theme.borderWidthNone
                    border.color: Theme.color("fileManagerControlBorder")
                    clip: true
                    z: 8
                    Behavior on width {
                        NumberAnimation {
                            duration: 160
                            easing.type: Easing.OutCubic
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Image {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: true
                            source: "image://themeicon/system-search-symbolic?v=" + root.iconRevision
                        }

                        TextField {
                            id: toolbarSearchField
                            Layout.fillWidth: true
                            visible: hostRoot.toolbarSearchExpanded
                            opacity: hostRoot.toolbarSearchExpanded ? 1.0 : 0.0
                            text: hostRoot.toolbarSearchText
                            placeholderText: "Search"
                            selectByMouse: true
                            color: Theme.color("textPrimary")
                            background: Item {}
                            onTextChanged: hostRoot.toolbarSearchText = text
                            onActiveFocusChanged: {
                                if (!activeFocus && String(text || "").length === 0) {
                                    hostRoot.toolbarSearchExpanded = false
                                }
                            }
                            Keys.onEscapePressed: {
                                if (String(text || "").length > 0) {
                                    text = ""
                                } else {
                                    hostRoot.closeToolbarSearch(false)
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !hostRoot.toolbarSearchExpanded
                        onClicked: hostRoot.openToolbarSearch(true)
                    }
                }
            }
        }

        RowLayout {
            visible: hostRoot.trashView
            spacing: 8

            Button {
                text: "Restore"
                enabled: hostRoot.selectedEntryIndex >= 0
                onClicked: hostRoot.restoreSelectedFromTrash()
            }

            Button {
                text: "Restore All"
                enabled: hostRoot.fileModel && Number(hostRoot.fileModel.count || 0) > 0
                onClicked: hostRoot.restoreAllFromTrash()
            }

            Button {
                text: hostRoot.selectedEntryIndex >= 0 ? "Delete Selected" : "Empty Trash"
                enabled: hostRoot.fileModel && Number(hostRoot.fileModel.count || 0) > 0
                onClicked: hostRoot.deleteSelectedOrEmptyTrash()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 7
        enabled: hostRoot.toolbarSearchExpanded
        hoverEnabled: false
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: function(mouse) {
            var p = mapToItem(toolbarSearchPanel, mouse.x, mouse.y)
            var insideSearch = (p.x >= 0 && p.y >= 0
                                && p.x <= toolbarSearchPanel.width
                                && p.y <= toolbarSearchPanel.height)
            if (insideSearch) {
                mouse.accepted = false
                return
            }
            hostRoot.toolbarSearchExpanded = false
            hostRoot.toolbarSearchText = ""
            toolbarSearchField.text = ""
            toolbarSearchField.focus = false
            mouse.accepted = true
        }
    }
}
