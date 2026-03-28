import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle
import Slm_Desktop
import "../applet" as Applet

Rectangle {
    id: root

    property var client: null
    property bool opened: false
    property var rows: []
    property int selectedIndex: 0

    signal closeRequested()

    width: Math.min(760, parent ? Math.round(parent.width * 0.62) : 760)
    height: Math.min(560, parent ? Math.round(parent.height * 0.7) : 560)
    radius: Theme.radiusWindow
    color: Theme.color("windowBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("panelBorder")
    visible: opened
    opacity: opened ? 1 : 0
    scale: opened ? 1.0 : 0.95
    focus: opened

    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

    function reload() {
        if (!client) {
            rows = []
            selectedIndex = 0
            return
        }
        var q = searchField.text.trim()
        rows = q.length > 0 ? client.search(q, 120) : client.getHistory(120)
        if (rows.length <= 0) {
            selectedIndex = -1
        } else if (selectedIndex < 0 || selectedIndex >= rows.length) {
            selectedIndex = 0
        }
    }

    function activateSelected() {
        if (!client || selectedIndex < 0 || selectedIndex >= rows.length) {
            return
        }
        var row = rows[selectedIndex]
        if (client.pasteItem(Number(row.id || 0))) {
            closeRequested()
        }
    }

    Keys.onPressed: function(event) {
        if (!opened) {
            return
        }
        if (event.key === Qt.Key_Escape) {
            event.accepted = true
            closeRequested()
            return
        }
        if (event.key === Qt.Key_Down) {
            event.accepted = true
            if (rows.length > 0) {
                selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
                listView.positionViewAtIndex(selectedIndex, ListView.Contain)
            }
            return
        }
        if (event.key === Qt.Key_Up) {
            event.accepted = true
            if (rows.length > 0) {
                selectedIndex = Math.max(0, selectedIndex - 1)
                listView.positionViewAtIndex(selectedIndex, ListView.Contain)
            }
            return
        }
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            event.accepted = true
            activateSelected()
        }
    }

    Component.onCompleted: reload()
    onOpenedChanged: {
        if (opened) {
            reload()
            Qt.callLater(function() { searchField.forceActiveFocus() })
        } else {
            searchField.text = ""
        }
    }

    Connections {
        target: root.client
        function onHistoryChanged() {
            if (root.opened) {
                root.reload()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.metric("spacingLg")
        spacing: Theme.metric("spacingSm")

        Label {
            Layout.fillWidth: true
            text: "Clipboard History"
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("semibold")
            color: Theme.color("textPrimary")
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Search clipboard"
            onTextChanged: root.reload()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusMd
            color: Theme.color("panelBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: Theme.metric("spacingXs")
                model: root.rows
                spacing: Theme.metric("spacingXxs")
                clip: true

                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    readonly property var row: modelData
                    width: ListView.view ? ListView.view.width : 200
                    height: Theme.metric("controlHeightLarge")
                    radius: Theme.radiusControl
                    color: (index === root.selectedIndex) ? Theme.color("accentSoft") : (mouse.containsMouse ? Theme.color("menuHoverBg") : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.metric("spacingSm")
                        anchors.rightMargin: Theme.metric("spacingSm")
                        spacing: Theme.metric("spacingSm")

                        Label {
                            Layout.fillWidth: true
                            text: row.preview || row.content || ""
                            color: Theme.color("textPrimary")
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("body")
                            elide: Text.ElideRight
                        }

                        Label {
                            text: String(row.type || "").toUpperCase()
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }
                    }

                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.selectedIndex = index
                            root.activateSelected()
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
    }
}
