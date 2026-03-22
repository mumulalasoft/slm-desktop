import QtQuick 2.15

Item {
    id: root

    required property var hostRoot
    property var fileModel: null
    property var quickTypeClearTimerRef: null

    Connections {
        target: root.fileModel
        ignoreUnknownSignals: true

        function onCurrentPathChanged() {
            if (!root.fileModel || root.fileModel.currentPath === undefined) {
                return
            }
            var p = String(root.fileModel.currentPath || "~")
            root.hostRoot.contentLoading = true
            root.hostRoot.quickTypeBuffer = ""
            if (root.quickTypeClearTimerRef && root.quickTypeClearTimerRef.stop) {
                root.quickTypeClearTimerRef.stop()
            }
            root.hostRoot.selectedSidebarPath = p
            root.hostRoot.updateTabPath(root.hostRoot.activeTabIndex, p)
            root.hostRoot.recordNavigationPath(p)
        }

        function onCountChanged() {
            root.hostRoot.contentLoading = false
        }

        function onDeepSearchRunningChanged() {
            root.hostRoot.syncDeepSearchBusyCursor()
            if (!root.fileModel || !root.fileModel.deepSearchRunning) {
                root.hostRoot.contentLoading = false
            }
        }
    }
}
