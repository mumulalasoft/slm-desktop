import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Desktop_Shell

Rectangle {
    id: toolbar
    property var fileModel: null
    property string viewMode: "grid"
    property string searchText: ""
    property string searchKind: "any" // any | file | dir
    property bool canGoPrevious: false
    property bool canGoNext: false
    property bool indexing: false
    property bool showIndexControls: false
    signal closeRequested()
    signal previousRequested()
    signal nextRequested()
    signal viewModeRequested(string mode)
    signal searchTextRequested(string text)
    signal searchKindRequested(string kind)
    signal reindexRequested()
    signal cancelIndexRequested()
    signal searchAccepted()

    readonly property var hostWindow: Window.window
    readonly property bool windowActive: hostWindow ? hostWindow.active : true
    readonly property bool windowMaximized: hostWindow
                                          ? hostWindow.visibility === Window.Maximized
                                            || hostWindow.visibility === Window.FullScreen
                                          : false

    function titleButtonIcon(kind, hovered, pressed) {
        var base = "qrc:/icons/titlebuttons/"
        var active = windowActive
        if (kind === "close") {
            if (!active) {
                return base + ((hovered || pressed) ? "titlebutton-close-backdrop-active.svg"
                                       : "titlebutton-close-backdrop.svg")
            }
            if (pressed) {
                return base + "titlebutton-close-active.svg"
            }
            return base + (hovered ? "titlebutton-close-hover.svg" : "titlebutton-close.svg")
        }
        if (kind === "minimize") {
            if (!active) {
                return base + ((hovered || pressed) ? "titlebutton-minimize-backdrop-active.svg"
                                       : "titlebutton-minimize-backdrop.svg")
            }
            if (pressed) {
                return base + "titlebutton-minimize-active.svg"
            }
            return base + (hovered ? "titlebutton-minimize-hover.svg" : "titlebutton-minimize.svg")
        }
        if (kind === "maximize") {
            if (windowMaximized) {
                if (!active) {
                    return base + ((hovered || pressed) ? "titlebutton-unmaximize-backdrop-active.svg"
                                           : "titlebutton-unmaximize-backdrop.svg")
                }
                if (pressed) {
                    return base + "titlebutton-unmaximize-active.svg"
                }
                return base + (hovered ? "titlebutton-unmaximize-hover.svg"
                                       : "titlebutton-unmaximize-active.svg")
            }
            if (!active) {
                return base + ((hovered || pressed) ? "titlebutton-maximize-backdrop-active.svg"
                                       : "titlebutton-maximize-backdrop.svg")
            }
            if (pressed) {
                return base + "titlebutton-maximize-active.svg"
            }
            return base + (hovered ? "titlebutton-maximize-hover.svg" : "titlebutton-maximize.svg")
        }
        return ""
    }

    function minimizeWindow() {
        if (hostWindow && hostWindow.showMinimized) {
            hostWindow.showMinimized()
        }
    }

    function toggleMaximizeWindow() {
        if (!hostWindow) {
            return
        }
        if (windowMaximized) {
            if (hostWindow.showNormal) {
                hostWindow.showNormal()
            } else {
                hostWindow.visibility = Window.Windowed
            }
            return
        }
        if (hostWindow.showMaximized) {
            hostWindow.showMaximized()
        } else {
            hostWindow.visibility = Window.Maximized
        }
    }

    function basename(pathValue) {
        var p = String(pathValue || "")
        while (p.length > 1 && p.endsWith("/")) {
            p = p.slice(0, p.length - 1)
        }
        var idx = p.lastIndexOf("/")
        return idx >= 0 ? p.slice(idx + 1) : p
    }

    function focusSearch() {
        searchAccepted()
    }

    color: Theme.color("surface")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("panelBorder")

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 10

        Row {
            spacing: 8
            Layout.alignment: Qt.AlignVCenter
            Item {
                width: 16
                height: 16
                scale: closeMouse.pressed ? 0.9 : (closeMouse.containsMouse ? 1.04 : 1.0)
                Behavior on scale {
                    NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
                }
                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: toolbar.titleButtonIcon("close", closeMouse.containsMouse, closeMouse.pressed)
                    opacity: closeMouse.pressed ? 0.9 : 1.0
                    Behavior on opacity {
                        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
                    }
                }
                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: toolbar.closeRequested()
                }
            }
            Item {
                width: 16
                height: 16
                scale: minimizeMouse.pressed ? 0.9 : (minimizeMouse.containsMouse ? 1.04 : 1.0)
                Behavior on scale {
                    NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
                }
                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: toolbar.titleButtonIcon("minimize", minimizeMouse.containsMouse, minimizeMouse.pressed)
                    opacity: minimizeMouse.pressed ? 0.9 : 1.0
                    Behavior on opacity {
                        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
                    }
                }
                MouseArea {
                    id: minimizeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: toolbar.minimizeWindow()
                }
            }
            Item {
                width: 16
                height: 16
                scale: maximizeMouse.pressed ? 0.9 : (maximizeMouse.containsMouse ? 1.04 : 1.0)
                Behavior on scale {
                    NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
                }
                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: toolbar.titleButtonIcon("maximize", maximizeMouse.containsMouse, maximizeMouse.pressed)
                    opacity: maximizeMouse.pressed ? 0.9 : 1.0
                    Behavior on opacity {
                        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
                    }
                }
                MouseArea {
                    id: maximizeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: toolbar.toggleMaximizeWindow()
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 98
            height: 36
            radius: Theme.radiusXxl
            color: Theme.color("fileManagerControlBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerControlBorder")
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 2
                ToolButton {
                    text: "\u2039"
                    enabled: toolbar.canGoPrevious
                    onClicked: toolbar.previousRequested()
                }
                ToolButton {
                    text: "\u203a"
                    enabled: toolbar.canGoNext
                    onClicked: toolbar.nextRequested()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: {
                var p = fileModel && fileModel.currentPath ? String(fileModel.currentPath) : "~"
                var n = toolbar.basename(p)
                return n && n.length > 0 ? n : p
            }
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("jumbo")
            font.family: Theme.fontFamilyDisplay
            font.weight: Theme.fontWeight("semibold")
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.preferredWidth: 86
            height: 36
            radius: Theme.radiusXxl
            color: Theme.color("fileManagerControlBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerControlBorder")
            clip: true
            readonly property int segmentIndex: toolbar.viewMode === "list" ? 1 : 0
            readonly property real innerMargin: 6
            readonly property real segmentWidth: (width - (innerMargin * 2)) / 2
            Rectangle {
                id: viewModeHighlight
                x: parent.innerMargin + (parent.segmentIndex * parent.segmentWidth)
                y: 4
                width: parent.segmentWidth
                height: parent.height - 8
                radius: Theme.radiusControlLarge
                color: Theme.color("fileManagerControlActive")
                Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            }
            Row {
                anchors.fill: parent
                anchors.leftMargin: parent.innerMargin
                anchors.rightMargin: parent.innerMargin
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: 0
                Item {
                    width: parent.width / 2
                    height: parent.height
                    Image {
                        id: gridModeIcon
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        fillMode: Image.PreserveAspectFit
                        source: "image://themeicon/view-grid-symbolic?v=" +
                                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                 ? ThemeIconController.revision : 0)
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: gridModeIcon.status !== Image.Ready
                        text: "\u25a6"
                        color: Theme.color("textPrimary")
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: toolbar.viewModeRequested("grid")
                    }
                }
                Item {
                    width: parent.width / 2
                    height: parent.height
                    Image {
                        id: listModeIcon
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        fillMode: Image.PreserveAspectFit
                        source: "image://themeicon/view-list-symbolic?v=" +
                                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                 ? ThemeIconController.revision : 0)
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: listModeIcon.status !== Image.Ready
                        text: "\u2261"
                        color: Theme.color("textPrimary")
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: toolbar.viewModeRequested("list")
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 36
            height: 36
            radius: Theme.radiusXxl
            color: Theme.color("fileManagerControlBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerControlBorder")
            ToolButton {
                anchors.fill: parent
                onClicked: toolbar.searchAccepted()
                contentItem: Image {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    fillMode: Image.PreserveAspectFit
                    source: "image://themeicon/system-search-symbolic?v=" +
                            ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                             ? ThemeIconController.revision : 0)
                }
            }
        }

        Rectangle {
            visible: toolbar.showIndexControls
            Layout.preferredWidth: 92
            height: 36
            radius: Theme.radiusXxl
            color: Theme.color("fileManagerControlBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerControlBorder")
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 2
                ToolButton {
                    Layout.fillWidth: true
                    text: "\u21bb"
                    onClicked: toolbar.reindexRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Reindex"
                }
                ToolButton {
                    Layout.fillWidth: true
                    enabled: toolbar.indexing
                    text: "\u2715"
                    onClicked: toolbar.cancelIndexRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Cancel Index"
                }
            }
        }
    }
}
