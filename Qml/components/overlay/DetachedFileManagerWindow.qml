import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../../apps/filemanager/FileManagerGlobalMenuController.js" as FileManagerGlobalMenuController
import "../shell/ShellUtils.js" as ShellUtils

Window {
    id: root

    required property var rootWindow
    required property var shellApi
    signal printRequested(string documentUri, string documentTitle, bool preferPdfOutput)
    property int panelHeight: 0
    property var fileModel: null

    readonly property var loadedItem: detachedFileManagerLoader.item
    readonly property int loaderStatus: detachedFileManagerLoader.status

    visible: !!shellApi && shellApi.detachedFileManagerVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    transientParent: rootWindow
    title: "Desktop File Manager"
    property bool applyingPolicy: false
    readonly property int resizeHandle: 8
    readonly property int shadowMargin: Theme.windowShadowMargin
    readonly property int topSafeY: (rootWindow ? rootWindow.y : 0) + panelHeight + 12
    readonly property int sideMargin: 20
    readonly property int bottomMargin: 20
    minimumWidth: 720
    minimumHeight: 460
    x: (rootWindow ? rootWindow.x : 0) + Math.round(((rootWindow ? rootWindow.width : width) - width) / 2)
    y: (rootWindow ? rootWindow.y : 0) + panelHeight + 16
    width: Math.min(Math.max(920, Math.round((rootWindow ? rootWindow.width : width) * 0.72)),
                    (rootWindow ? rootWindow.width : width) - 40)
    height: Math.min(Math.max(560, Math.round((rootWindow ? rootWindow.height : height) * 0.72)),
                     (rootWindow ? rootWindow.height : height) - panelHeight - 28)

    function setLoaderActive(active) {
        detachedFileManagerLoader.active = !!active
    }

    function restartWatchdog() {
        detachedFileManagerWatchdog.restart()
    }

    function stopWatchdog() {
        detachedFileManagerWatchdog.stop()
    }

    function openPathIfReady(pathValue) {
        if (detachedFileManagerLoader.item && detachedFileManagerLoader.item.openPath) {
            detachedFileManagerLoader.item.openPath(String(pathValue || "~"))
            return detachedFileManagerLoader.item
        }
        return null
    }

    function showPropertiesIfReady(pathValue) {
        var p = String(pathValue || "").trim()
        if (p.length <= 0) {
            return false
        }
        if (detachedFileManagerLoader.item && detachedFileManagerLoader.item.showPropertiesForPath) {
            detachedFileManagerLoader.item.showPropertiesForPath(p)
            return true
        }
        return false
    }

    function closeContextMenusIfReady() {
        if (detachedFileManagerLoader.item && detachedFileManagerLoader.item.closeAllContextMenus) {
            detachedFileManagerLoader.item.closeAllContextMenus()
        }
    }

    function enforceGeometryPolicy() {
        if (applyingPolicy) {
            return
        }
        applyingPolicy = true

        // Respect window-manager maximize/fullscreen state.
        // Geometry clamping below applies only to windowed mode.
        if (visibility === Window.Maximized || visibility === Window.FullScreen) {
            applyingPolicy = false
            return
        }

        var minX = (rootWindow ? rootWindow.x : 0) + sideMargin
        var maxX = (rootWindow ? rootWindow.x + rootWindow.width : width) - sideMargin
        var minY = topSafeY
        var maxY = (rootWindow ? rootWindow.y + rootWindow.height : height) - bottomMargin
        var maxW = Math.max(minimumWidth, maxX - minX)
        var maxH = Math.max(minimumHeight, maxY - minY)

        if (width > maxW) {
            width = maxW
        }
        if (height > maxH) {
            height = maxH
        }
        if (width < minimumWidth) {
            width = minimumWidth
        }
        if (height < minimumHeight) {
            height = minimumHeight
        }

        if (x < minX) {
            x = minX
        }
        if ((x + width) > maxX) {
            x = Math.max(minX, maxX - width)
        }
        if (y < minY) {
            y = minY
        }
        if ((y + height) > maxY) {
            y = Math.max(minY, maxY - height)
        }

        applyingPolicy = false
    }

    onVisibleChanged: {
        if (visible) {
            if (visibility !== Window.Windowed) {
                visibility = Window.Windowed
            }
            x = (rootWindow ? rootWindow.x : 0) + Math.round(((rootWindow ? rootWindow.width : width) - width) / 2)
            y = (rootWindow ? rootWindow.y : 0) + panelHeight + 16
            enforceGeometryPolicy()
            requestActivate()
        }
        FileManagerGlobalMenuController.syncOverride(shellApi, root)
    }
    onActiveChanged: {
        FileManagerGlobalMenuController.syncOverride(shellApi, root)
        if (!active) {
            closeContextMenusIfReady()
        }
    }
    onVisibilityChanged: enforceGeometryPolicy()
    onXChanged: enforceGeometryPolicy()
    onYChanged: enforceGeometryPolicy()
    onWidthChanged: enforceGeometryPolicy()
    onHeightChanged: enforceGeometryPolicy()

    function canSystemResize() {
        return visibility !== Window.Maximized && visibility !== Window.FullScreen
    }

    function trySystemResize(edges) {
        if (!canSystemResize() || !startSystemResize) {
            return
        }
        startSystemResize(edges)
    }

    function trySystemMove() {
        if (visibility === Window.Maximized || visibility === Window.FullScreen || !startSystemMove) {
            return
        }
        startSystemMove()
    }

    function toggleMaximizeRestore() {
        if (visibility === Window.Maximized || visibility === Window.FullScreen) {
            if (showNormal) {
                showNormal()
            } else {
                visibility = Window.Windowed
            }
            return
        }
        if (showMaximized) {
            showMaximized()
        } else {
            visibility = Window.Maximized
        }
    }

    Connections {
        target: rootWindow
        ignoreUnknownSignals: true
        function onWidthChanged() { root.enforceGeometryPolicy() }
        function onHeightChanged() { root.enforceGeometryPolicy() }
        function onXChanged() { root.enforceGeometryPolicy() }
        function onYChanged() { root.enforceGeometryPolicy() }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Theme.radiusWindow + 4
        color: Qt.rgba(0, 0, 0, Theme.windowShadowOuterOpacity)
        visible: root.visibility !== Window.Maximized
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 3
        radius: Theme.radiusWindow + 2
        color: Qt.rgba(0, 0, 0, Theme.windowShadowInnerOpacity)
        visible: root.visibility !== Window.Maximized
    }

    Rectangle {
        id: detachedFileManagerFrame
        anchors.fill: parent
        anchors.margins: root.visibility === Window.Maximized ? 0 : root.shadowMargin
        radius: Theme.radiusWindow
        antialiasing: true
        clip: true
        color: Theme.color("fileManagerWindowBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerWindowBorder")

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: Theme.borderWidthThin
            border.color: Qt.rgba(0, 0, 0, Theme.windowInnerStrokeOpacity)
            antialiasing: true
        }

        Loader {
            id: detachedFileManagerLoader
            anchors.fill: parent
            active: false
            asynchronous: true
            source: "qrc:/qt/qml/Slm_Desktop/Qml/apps/filemanager/FileManagerWindow.qml"
            onLoaded: {
                if (!item) {
                    shellApi.detachedFileManagerLoadFailed = true
                    return
                }
                if (item.hasOwnProperty("fileModel")) {
                    item.fileModel = root.fileModel
                }
                if (item.openPath) {
                    item.openPath(shellApi.detachedFileManagerPath)
                }
                shellApi.fileManagerContent = item
                shellApi.detachedFileManagerLoadFailed = false
                detachedFileManagerWatchdog.stop()
                if (shellApi.pendingDetachedFileManagerPropertiesPath.length > 0
                        && item.showPropertiesForPath) {
                    item.showPropertiesForPath(shellApi.pendingDetachedFileManagerPropertiesPath)
                    shellApi.pendingDetachedFileManagerPropertiesPath = ""
                }
            }
            onStatusChanged: {
                if (status === Loader.Error) {
                    var err = detachedFileManagerLoader.errorString()
                    shellApi.detachedFileManagerLoadFailed = true
                    detachedFileManagerWatchdog.stop()
                    console.error("Main: detached loader error=", err)
                }
            }
        }
    }

    Connections {
        target: detachedFileManagerLoader.item
        ignoreUnknownSignals: true
        function onPrintRequested(documentUri, documentTitle, preferPdfOutput) {
            root.printRequested(String(documentUri || ""),
                                String(documentTitle || "Document"),
                                !!preferPdfOutput)
        }
    }

    MouseArea {
        id: topDragArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 250
        anchors.rightMargin: 260
        height: 12
        cursorShape: Qt.SizeAllCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.visibility !== Window.Maximized
        onPressed: root.trySystemMove()
        onDoubleClicked: root.toggleMaximizeRestore()
        z: 10
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.resizeHandle
        anchors.rightMargin: root.resizeHandle
        height: root.resizeHandle
        cursorShape: Qt.SizeVerCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.TopEdge)
        z: 20
    }
    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.resizeHandle
        anchors.rightMargin: root.resizeHandle
        height: root.resizeHandle
        cursorShape: Qt.SizeVerCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.BottomEdge)
        z: 20
    }
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.resizeHandle
        anchors.bottomMargin: root.resizeHandle
        width: root.resizeHandle
        cursorShape: Qt.SizeHorCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.LeftEdge)
        z: 20
    }
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.resizeHandle
        anchors.bottomMargin: root.resizeHandle
        width: root.resizeHandle
        cursorShape: Qt.SizeHorCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.RightEdge)
        z: 20
    }
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        width: root.resizeHandle + 2
        height: root.resizeHandle + 2
        cursorShape: Qt.SizeFDiagCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.LeftEdge | Qt.TopEdge)
        z: 21
    }
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        width: root.resizeHandle + 2
        height: root.resizeHandle + 2
        cursorShape: Qt.SizeBDiagCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.RightEdge | Qt.TopEdge)
        z: 21
    }
    MouseArea {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.resizeHandle + 2
        height: root.resizeHandle + 2
        cursorShape: Qt.SizeBDiagCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.LeftEdge | Qt.BottomEdge)
        z: 21
    }
    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.resizeHandle + 2
        height: root.resizeHandle + 2
        cursorShape: Qt.SizeFDiagCursor
        acceptedButtons: Qt.LeftButton
        enabled: root.canSystemResize()
        onPressed: root.trySystemResize(Qt.RightEdge | Qt.BottomEdge)
        z: 21
    }

    Rectangle {
        anchors.fill: parent
        visible: !!shellApi
                 && shellApi.detachedFileManagerVisible
                 && (detachedFileManagerLoader.status !== Loader.Ready || shellApi.detachedFileManagerLoadFailed)
        radius: detachedFileManagerFrame.radius
        color: Theme.color("fileManagerWindowBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerWindowBorder")
        z: 3
        Text {
            anchors.centerIn: parent
            text: (shellApi && shellApi.detachedFileManagerLoadFailed)
                  ? "File Manager failed to load" : "Loading File Manager..."
            color: Theme.color("textSecondary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
        }
    }

    Connections {
        target: detachedFileManagerLoader.item
        ignoreUnknownSignals: true
        function onCloseRequested() {
            shellApi.detachedFileManagerVisible = false
        }
        function onOpenInNewWindowRequested(path) {
            ShellUtils.openDetachedFileManager(shellApi, path)
        }
    }

    Timer {
        id: detachedFileManagerWatchdog
        interval: 8000
        repeat: false
        onTriggered: {
            if (!shellApi.detachedFileManagerVisible) {
                return
            }
            if (detachedFileManagerLoader.status === Loader.Ready) {
                return
            }
            shellApi.detachedFileManagerLoadFailed = true
            console.error("Main: detached loader timeout status=", detachedFileManagerLoader.status)
        }
    }
}
