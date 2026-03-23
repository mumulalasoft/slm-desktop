import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property bool pathEditMode: false
    property string currentDir: ""
    property int iconRevision: 0
    property var breadcrumbSegments: []

    readonly property bool editorActiveFocus: portalDirField.activeFocus
    readonly property string editorText: portalDirField.text

    signal pathEditModeChangeRequested(bool value)
    signal loadDirectoryRequested(string path)

    width: parent ? parent.width : 0
    height: 34

    function focusEditorAndSelectAll() {
        pathEditModeChangeRequested(true)
        portalDirField.forceActiveFocus()
        portalDirField.selectAll()
    }

    function clearEditorFocus() {
        portalDirField.focus = false
    }

    Row {
        anchors.fill: parent
        spacing: 6

        Item {
            id: pathStackArea
            width: Math.max(0, parent.width - 42)
            height: parent.height

            TextField {
                id: portalDirField
                anchors.fill: parent
                opacity: root.pathEditMode ? 1 : 0
                visible: opacity > 0.01
                enabled: root.pathEditMode
                text: root.currentDir
                color: Theme.color("textPrimary")
                selectionColor: Theme.color("selectedItem")
                selectedTextColor: Theme.color("selectedItemText")
                placeholderText: "Directory"
                Behavior on opacity {
                    NumberAnimation { duration: 140; easing.type: Easing.InOutQuad }
                }
                onAccepted: {
                    root.loadDirectoryRequested(text)
                    root.pathEditModeChangeRequested(false)
                }
            }

            Flickable {
                anchors.fill: parent
                opacity: root.pathEditMode ? 0 : 1
                visible: opacity > 0.01
                enabled: !root.pathEditMode
                contentWidth: portalPathBreadcrumbRow.width
                contentHeight: height
                clip: true
                Behavior on opacity {
                    NumberAnimation { duration: 140; easing.type: Easing.InOutQuad }
                }

                Row {
                    id: portalPathBreadcrumbRow
                    height: parent.height
                    spacing: 2
                    Repeater {
                        model: root.breadcrumbSegments
                        delegate: Item {
                            required property var modelData
                            height: parent.height
                            width: crumbButton.width

                            Button {
                                id: crumbButton
                                text: String(modelData.label || "")
                                height: 22
                                width: Math.max(36, implicitWidth + 4)
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: root.loadDirectoryRequested(String(modelData.path || "~"))
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    propagateComposedEvents: true
                    onPressed: mouse.accepted = false
                    onClicked: mouse.accepted = false
                    onDoubleClicked: root.focusEditorAndSelectAll()
                }
            }
        }

        Button {
            id: portalPathModeButton
            width: 36
            height: 30
            text: ""
            property bool editWasActiveOnPress: false
            icon.width: 16
            icon.height: 16
            icon.source: "image://themeicon/"
                         + (root.pathEditMode ? "object-select-symbolic" : "document-edit-symbolic")
                         + "?v=" + root.iconRevision
            onPressed: editWasActiveOnPress = root.pathEditMode
            onClicked: {
                if (editWasActiveOnPress) {
                    root.pathEditModeChangeRequested(false)
                    root.forceActiveFocus()
                    return
                }
                root.focusEditorAndSelectAll()
            }
        }
    }

    MouseArea {
        id: autoCloseArea
        x: pathStackArea.x
        y: pathStackArea.y
        width: pathStackArea.width
        height: pathStackArea.height
        z: 100
        visible: root.pathEditMode
        enabled: visible
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true
        onPressed: function(mouse) {
            var local = portalDirField.mapFromItem(autoCloseArea, mouse.x, mouse.y)
            var insideEditor = local.x >= 0 && local.y >= 0
                    && local.x < portalDirField.width && local.y < portalDirField.height
            if (!insideEditor) {
                root.pathEditModeChangeRequested(false)
            }
            mouse.accepted = false
        }
    }
}
