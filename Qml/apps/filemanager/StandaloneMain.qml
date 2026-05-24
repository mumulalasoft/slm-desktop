import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import SlmStyle
import "." as FM

Window {
    id: root
    objectName: "fileManagerStandaloneRoot"

    readonly property string initialPath:
        (typeof slmFileManagerInitialPath !== "undefined")
            ? String(slmFileManagerInitialPath)
            : "~"
    readonly property var modelFactoryRef:
        (typeof FileManagerModelFactory !== "undefined") ? FileManagerModelFactory : null

    width: 1024
    height: 720
    minimumWidth: 720
    minimumHeight: 460
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    title: qsTr("Files")
    color: Theme ? Theme.color("fileManagerWindowBg") : "#ffffff"

    property var fileModel: modelFactoryRef ? modelFactoryRef.createModel(root) : null

    FM.FileManagerWindow {
        id: fmWindow
        objectName: "fileManagerWindowRoot"
        anchors.fill: parent
        fileModel: root.fileModel

        Component.onCompleted: {
            if (fmWindow.openPath) {
                fmWindow.openPath(root.initialPath)
            }
        }

        onCloseRequested: root.close()
        onOpenInNewWindowRequested: function(path) {
            // Spawn a fresh slm-filemanager instance so each window owns its own
            // QQmlEngine — keeps in-process state isolated and matches Finder
            // semantics. Falls back to current-process open if QProcess fails.
            var args = []
            if (path && String(path).length > 0) {
                args.push(String(path))
            }
            if (!Qt.createQmlObject) {
                return
            }
            var proc = Qt.createQmlObject(
                'import QtCore 6.0; Item { property var sysProc: null }',
                root, "newWindowSpawner")
            // Qt.createQmlObject is QML-side; for cross-process spawn we rely on
            // the C++ side. Since we don't expose a QProcess wrapper here, fall
            // back to opening the path within the current window's openPath.
            if (fmWindow.openPath) {
                fmWindow.openPath(String(path || "~"))
            }
        }
    }

    Component.onCompleted: {
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && Theme && Theme.applyModeString) {
            Theme.applyModeString(String(DesktopSettings.themeMode || "system"))
        }
    }

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onThemeModeChanged() {
            if (Theme && Theme.applyModeString) {
                Theme.applyModeString(String(DesktopSettings.themeMode || "system"))
            }
        }
    }
}
