import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle

Item {
    id: root

    property var popupHost: null
    property var clipboardClient: (typeof ClipboardServiceClient !== "undefined") ? ClipboardServiceClient : null
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    property bool popupOpen: popupHint || menu.opened

    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingXs")

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) {
            return
        }
        popupHint = true
        Qt.callLater(function() {
            menu.open()
        })
    }

    ToolButton {
        id: button
        anchors.fill: parent
        padding: 0
        contentItem: Item {
            implicitWidth: root.iconSize
            implicitHeight: root.iconSize
            IconImage {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: "image://themeicon/edit-paste-symbolic?v=" +
                        ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                         ? ThemeIconController.revision : 0)
                color: Theme.color("textOnGlass")
            }
        }
        background: Rectangle {
            radius: Theme.radiusControl
            color: button.down ? Theme.color("controlBgPressed") : (button.hovered ? Theme.color("accentSoft") : "transparent")
            border.width: Theme.borderWidthThin
            border.color: button.hovered ? Theme.color("panelBorder") : "transparent"
            Behavior on color {
                enabled: root.microAnimationAllowed()
                ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
            Behavior on border.color {
                enabled: root.microAnimationAllowed()
                ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
        }
        onClicked: {
            if (menu.opened || menu.visible) {
                root.lastMenuCloseMs = Date.now()
                menu.close()
                return
            }
            root.openMenuSafely()
        }
    }

    IndicatorMenu {
        id: menu
        anchorItem: button
        popupGap: root.popupGap
        popupWidth: Math.max(360, Theme.metric("popupWidthL"))
        onAboutToShow: root.popupHint = false
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: "Clipboard"
                emphasized: true
            }
        }

        MenuItem {
            enabled: false
            height: Math.max(220, Theme.metric("popupHeightM"))
            contentItem: Item {
                id: appletContent

                property var client: root.clipboardClient
                property string searchText: ""
                property int maxItems: 40
                property var rows: []

                function reload() {
                    if (!client) {
                        rows = []
                        return
                    }
                    var q = String(searchText || "").trim()
                    rows = q.length > 0
                            ? client.search(q, maxItems)
                            : client.getHistory(maxItems)
                }

                function rowTypeLabel(row) {
                    var t = String(row && row.type ? row.type : "").toUpperCase()
                    return t === "" ? "TEXT" : t
                }

                Connections {
                    target: appletContent.client
                    function onHistoryChanged() { appletContent.reload() }
                }

                onClientChanged: reload()
                onSearchTextChanged: reload()
                Component.onCompleted: reload()

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.metric("spacingSm")

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.metric("spacingSm")

                        Item { Layout.fillWidth: true }

                        ToolButton {
                            text: "Clear"
                            enabled: !!appletContent.client && appletContent.rows.length > 0
                            onClicked: {
                                if (appletContent.client && appletContent.client.clearHistory()) {
                                    appletContent.reload()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Theme.radiusMd
                        color: Theme.color("panelBg")

                        ListView {
                            id: listView
                            anchors.fill: parent
                            anchors.margins: Theme.metric("spacingXs")
                            clip: true
                            spacing: Theme.metric("spacingXxs")
                            model: appletContent.rows

                            delegate: Rectangle {
                                id: rowRect
                                required property var modelData
                                readonly property var row: modelData
                                readonly property bool pinned: !!row.isPinned
                                width: ListView.view ? ListView.view.width : 200
                                height: Theme.metric("controlHeightLarge")
                                radius: Theme.radiusControl
                                color: rowMouse.pressed ? Theme.color("controlBgPressed") : (rowMouse.containsMouse ? Theme.color("accentSoft") : "transparent")
                                Behavior on color {
                                    enabled: root.microAnimationAllowed()
                                    ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.metric("spacingSm")
                                    anchors.rightMargin: Theme.metric("spacingSm")
                                    spacing: Theme.metric("spacingSm")

                                    Label {
                                        text: row.preview || row.content || ""
                                        Layout.fillWidth: true
                                        color: Theme.color("textPrimary")
                                        font.family: Theme.fontFamilyUi
                                        font.pixelSize: Theme.fontSize("body")
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: appletContent.rowTypeLabel(row)
                                        color: Theme.color("textSecondary")
                                        font.family: Theme.fontFamilyUi
                                        font.pixelSize: Theme.fontSize("small")
                                    }

                                    Label {
                                        visible: rowRect.pinned
                                        text: "\u2605"
                                        color: Theme.color("accent")
                                        font.pixelSize: Theme.fontSize("body")
                                    }
                                }

                                MouseArea {
                                    id: rowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: function(mouse) {
                                        if (!appletContent.client) return
                                        if (mouse.button === Qt.RightButton) {
                                            rowMenu.popup()
                                            return
                                        }
                                        if (appletContent.client.pasteItem(Number(row.id || 0))) {
                                            menu.close()
                                        }
                                    }
                                }

                                Menu {
                                    id: rowMenu
                                    y: rowRect.height
                                    MenuItem {
                                        text: rowRect.pinned ? "Unpin" : "Pin"
                                        onTriggered: {
                                            if (appletContent.client)
                                                appletContent.client.pinItem(Number(row.id || 0), !rowRect.pinned)
                                        }
                                    }
                                    MenuItem {
                                        text: "Delete"
                                        onTriggered: {
                                            if (appletContent.client)
                                                appletContent.client.deleteItem(Number(row.id || 0))
                                        }
                                    }
                                }
                            }

                            ScrollBar.vertical: ScrollBar {}

                            Item {
                                anchors.centerIn: parent
                                visible: listView.count <= 0
                                width: parent.width
                                height: implicitHeight
                                Label {
                                    anchors.centerIn: parent
                                    text: "Clipboard history is empty"
                                    color: Theme.color("textSecondary")
                                    font.family: Theme.fontFamilyUi
                                    font.pixelSize: Theme.fontSize("body")
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
