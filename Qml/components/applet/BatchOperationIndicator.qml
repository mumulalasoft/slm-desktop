import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root
    property var fileManagerApi: FileManagerApi
    readonly property bool activeOperation: !!(root.fileManagerApi && root.fileManagerApi.batchOperationActive)
    property bool holdVisible: false
    property bool popupHint: false
    property bool popupOpen: popupHint || opMenu.opened
    property bool autoOpenWhenActive: true
    readonly property int compactH: Theme.metric("controlHeightCompact")
    readonly property int regularH: Theme.metric("controlHeightRegular")
    readonly property int largeH: Theme.metric("controlHeightLarge")
    readonly property int spaceXs: Theme.metric("spacingXs")
    readonly property int spaceSm: Theme.metric("spacingSm")
    readonly property int spaceMd: Theme.metric("spacingMd")
    readonly property int spaceLg: Theme.metric("spacingLg")
    visible: opacity > 0.01
    implicitWidth: indicatorButton.implicitWidth
    implicitHeight: indicatorButton.implicitHeight
    opacity: (activeOperation || holdVisible) ? 1 : 0
    scale: (activeOperation || holdVisible) ? 1 : 0.9

    Behavior on opacity {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Timer {
        id: popupHintTimer
        interval: 1200
        repeat: false
        onTriggered: root.popupHint = false
    }

    function percentText() {
        var total = root.displayTotalValue()
        if (total <= 0) {
            return "0%"
        }
        var p = Math.round(root.displayProgressValue() * 100)
        return String(p) + "%"
    }

    function humanBytes(value) {
        var b = Number(value || 0)
        if (b <= 0) {
            return "0 B"
        }
        var units = ["B", "KB", "MB", "GB", "TB"]
        var i = 0
        while (b >= 1024 && i < units.length - 1) {
            b /= 1024
            i++
        }
        var fixed = (b >= 100 || i === 0) ? 0 : 1
        return b.toFixed(fixed) + " " + units[i]
    }

    function humanDuration(seconds) {
        var s = Math.max(0, Math.round(Number(seconds || 0)))
        var h = Math.floor(s / 3600)
        var m = Math.floor((s % 3600) / 60)
        var sec = s % 60
        if (h > 0) {
            return String(h) + "h " + String(m) + "m"
        }
        if (m > 0) {
            return String(m) + "m " + String(sec) + "s"
        }
        return String(sec) + "s"
    }

    property string activeOpId: ""
    property real startedAtMs: 0
    property real etaSeconds: -1
    property real speedBytesPerSec: 0
    property real lastSampleMs: 0
    property real lastSampleCurrent: 0
    property var failedItems: []
    property real lastProgressValue: 0
    property real lastCurrentValue: 0
    property real lastTotalValue: 0
    property string lastOperationType: ""
    property bool pendingBatchNotify: false
    property string pendingBatchOperation: ""
    property bool pendingBatchOk: false
    property real pendingBatchProcessed: 0
    property real pendingBatchTotal: 0
    property string pendingBatchError: ""
    property real progressUiIdleSinceMs: 0

    function positionOpMenu() {
        if (!opMenu || !indicatorButton) {
            return
        }
        var pt = opMenu.parent
                ? indicatorButton.mapToItem(opMenu.parent, 0, indicatorButton.height + root.spaceXs)
                : Qt.point(0, indicatorButton.height + root.spaceXs)
        opMenu.x = Math.round(pt.x + (indicatorButton.width - opMenu.width) * 0.5)
        opMenu.y = Math.round(pt.y)
    }

    function openOpMenu() {
        root.positionOpMenu()
        if (!opMenu.opened) {
            opMenu.open()
        }
    }

    function displayProgressValue() {
        if (root.activeOperation && root.fileManagerApi) {
            return Math.max(0, Math.min(1, Number(root.fileManagerApi.batchOperationProgress || 0)))
        }
        return Math.max(0, Math.min(1, Number(root.lastProgressValue || 0)))
    }

    function displayCurrentValue() {
        if (root.activeOperation && root.fileManagerApi) {
            return Number(root.fileManagerApi.batchOperationCurrent || 0)
        }
        return Number(root.lastCurrentValue || 0)
    }

    function displayTotalValue() {
        if (root.activeOperation && root.fileManagerApi) {
            return Number(root.fileManagerApi.batchOperationTotal || 0)
        }
        return Number(root.lastTotalValue || 0)
    }

    function displayOperationType() {
        if (root.activeOperation && root.fileManagerApi) {
            return String(root.fileManagerApi.batchOperationType || "")
        }
        return String(root.lastOperationType || "")
    }

    function captureProgressSnapshot(opOverride, currentOverride, totalOverride) {
        var op = (opOverride !== undefined && opOverride !== null)
                ? String(opOverride || "")
                : root.displayOperationType()
        var current = (currentOverride !== undefined && currentOverride !== null)
                ? Number(currentOverride || 0)
                : root.displayCurrentValue()
        var total = (totalOverride !== undefined && totalOverride !== null)
                ? Number(totalOverride || 0)
                : root.displayTotalValue()
        root.lastOperationType = op
        root.lastCurrentValue = current
        root.lastTotalValue = total
        root.lastProgressValue = total > 0 ? Math.max(0, Math.min(1, current / total)) : 0
    }

    function updateEta() {
        if (!root.fileManagerApi || !root.fileManagerApi.batchOperationActive) {
            etaSeconds = -1
            speedBytesPerSec = 0
            lastSampleMs = 0
            lastSampleCurrent = 0
            return
        }
        var id = String(root.fileManagerApi.batchOperationId || "")
        if (id !== activeOpId) {
            activeOpId = id
            startedAtMs = Date.now()
            etaSeconds = -1
            speedBytesPerSec = 0
            lastSampleMs = startedAtMs
            lastSampleCurrent = Number(root.fileManagerApi.batchOperationCurrent || 0)
            return
        }
        var total = Number(root.fileManagerApi.batchOperationTotal || 0)
        var current = Number(root.fileManagerApi.batchOperationCurrent || 0)
        var nowMs = Date.now()
        if (lastSampleMs > 0 && nowMs > lastSampleMs) {
            var dt = (nowMs - lastSampleMs) / 1000.0
            var dc = current - lastSampleCurrent
            if (dt > 0 && dc >= 0) {
                var inst = dc / dt
                if (speedBytesPerSec <= 0) {
                    speedBytesPerSec = inst
                } else {
                    speedBytesPerSec = speedBytesPerSec * 0.72 + inst * 0.28
                }
            }
        }
        lastSampleMs = nowMs
        lastSampleCurrent = current
        if (total <= 0 || current <= 0 || startedAtMs <= 0) {
            etaSeconds = -1
            return
        }
        var elapsedSec = (Date.now() - startedAtMs) / 1000.0
        if (elapsedSec <= 0.2) {
            etaSeconds = -1
            return
        }
        var speed = current / elapsedSec
        if (speedBytesPerSec > 0 && total > (8 * 1024 * 1024)) {
            speed = speedBytesPerSec
        }
        if (speed <= 0) {
            etaSeconds = -1
            return
        }
        etaSeconds = (total - current) / speed
    }

    function refreshFailedItems() {
        if (!root.fileManagerApi || !root.fileManagerApi.failedBatchItems) {
            failedItems = []
            return
        }
        failedItems = root.fileManagerApi.failedBatchItems()
    }

    function notifyBatchFinished(operation, ok, processed, total, error) {
        if (typeof NotificationManager === "undefined" || !NotificationManager || !NotificationManager.Notify) {
            return
        }
        var op = String(operation || "")
        var opTitle = "Operation"
        var iconName = "document-save-symbolic"
        if (op === "copy") {
            opTitle = "Copy"
            iconName = "edit-copy-symbolic"
        } else if (op === "move") {
            opTitle = "Move"
            iconName = "go-jump-symbolic"
        } else if (op === "delete") {
            opTitle = "Delete"
            iconName = "edit-delete-symbolic"
        } else if (op === "trash") {
            opTitle = "Move to Trash"
            iconName = "user-trash-symbolic"
        } else if (op === "restore") {
            opTitle = "Restore"
            iconName = "edit-undo-symbolic"
        }

        var processedNum = Number(processed || 0)
        var totalNum = Number(total || 0)
        var body = ""
        if (totalNum > (8 * 1024 * 1024)) {
            body = humanBytes(processedNum) + " / " + humanBytes(totalNum)
        } else if (totalNum > 0) {
            body = String(processedNum) + " / " + String(totalNum)
        }

        if (ok) {
            if (body.length === 0) {
                body = "Completed successfully"
            }
            NotificationManager.Notify("Desktop File Manager", 0, iconName,
                                       opTitle + " completed", body, [], {}, 5000)
            return
        }

        var errText = String(error || "")
        if (errText.length === 0) {
            errText = "Completed with errors"
        }
        NotificationManager.Notify("Desktop File Manager", 0, "dialog-warning-symbolic",
                                   opTitle + " finished with errors",
                                   errText + (body.length > 0 ? (" (" + body + ")") : ""),
                                   [], {}, 5000)
    }

    function queueBatchFinishedNotification(operation, ok, processed, total, error) {
        pendingBatchNotify = true
        pendingBatchOperation = String(operation || "")
        pendingBatchOk = !!ok
        pendingBatchProcessed = Number(processed || 0)
        pendingBatchTotal = Number(total || 0)
        pendingBatchError = String(error || "")
        maybeFlushBatchFinishedNotification()
    }

    function maybeFlushBatchFinishedNotification() {
        if (!pendingBatchNotify) {
            return
        }
        var uiBusy = root.activeOperation ||
                root.holdVisible ||
                root.popupHint ||
                root.popupOpen ||
                (opMenu && (opMenu.opened || opMenu.visible)) ||
                (cancelDialog && cancelDialog.visible) ||
                (failedDialog && failedDialog.visible) ||
                root.visible ||
                root.opacity > 0.01
        if (uiBusy) {
            progressUiIdleSinceMs = 0
            return
        }
        if (progressUiIdleSinceMs <= 0) {
            progressUiIdleSinceMs = Date.now()
            return
        }
        if ((Date.now() - progressUiIdleSinceMs) < 250) {
            return
        }
        notifyBatchFinished(pendingBatchOperation,
                            pendingBatchOk,
                            pendingBatchProcessed,
                            pendingBatchTotal,
                            pendingBatchError)
        pendingBatchNotify = false
        progressUiIdleSinceMs = 0
    }

    Timer {
        id: visibleHoldTimer
        interval: 4500
        repeat: false
        onTriggered: {
            if (!root.activeOperation) {
                root.holdVisible = false
            }
        }
    }

    Timer {
        id: inactiveCloseDebounce
        interval: 3000
        repeat: false
        onTriggered: {
            if (!root.activeOperation && opMenu.opened) {
                opMenu.close()
            }
        }
    }

    Timer {
        id: pendingNotifyFlushTimer
        interval: 120
        repeat: true
        running: root.pendingBatchNotify
        onTriggered: root.maybeFlushBatchFinishedNotification()
    }

    Connections {
        target: Qt.application
        ignoreUnknownSignals: true
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive) {
                return
            }
            inactiveCloseDebounce.stop()
            if (opMenu.opened) {
                opMenu.close()
            }
        }
    }

    onActiveOperationChanged: {
        if (root.activeOperation) {
            inactiveCloseDebounce.stop()
            root.holdVisible = true
            visibleHoldTimer.restart()
            if (root.autoOpenWhenActive && !opMenu.opened) {
                root.popupHint = true
                popupHintTimer.restart()
                Qt.callLater(function() {
                    if (!opMenu.opened) {
                        root.openOpMenu()
                    }
                })
            }
        } else {
            visibleHoldTimer.restart()
            inactiveCloseDebounce.restart()
            Qt.callLater(root.maybeFlushBatchFinishedNotification)
        }
    }

    onHoldVisibleChanged: {
        if (!holdVisible) {
            Qt.callLater(root.maybeFlushBatchFinishedNotification)
        }
    }

    onPopupOpenChanged: {
        if (!popupOpen) {
            Qt.callLater(root.maybeFlushBatchFinishedNotification)
        }
    }

    onVisibleChanged: {
        if (!visible) {
            Qt.callLater(root.maybeFlushBatchFinishedNotification)
        }
    }

    Timer {
        interval: 500
        running: root.activeOperation
        repeat: true
        onTriggered: root.updateEta()
    }

    Connections {
        target: FileManagerApi
        ignoreUnknownSignals: true
        function onBatchOperationStateChanged() {
            if (FileManagerApi &&
                    (String(FileManagerApi.batchOperationId || "").length > 0 ||
                     !!FileManagerApi.batchOperationActive)) {
                root.holdVisible = true
                visibleHoldTimer.restart()
            }
            root.captureProgressSnapshot()
            if (!root.activeOperation) {
                inactiveCloseDebounce.restart()
                cancelDialog.close()
                failedDialog.close()
                root.activeOpId = ""
                root.etaSeconds = -1
                root.speedBytesPerSec = 0
                root.lastSampleMs = 0
                root.lastSampleCurrent = 0
            }
            root.updateEta()
            progressRing.requestPaint()
        }
        function onFileBatchOperationFinished(operation, ok, processed, total, error) {
            root.captureProgressSnapshot(String(operation || ""),
                                         Number(processed || 0),
                                         Number(total || 0))
            root.holdVisible = true
            visibleHoldTimer.restart()
            root.queueBatchFinishedNotification(operation, ok, processed, total, error)
            root.updateEta()
            progressRing.requestPaint()
        }
        function onFailedBatchItemsChanged() {
            root.refreshFailedItems()
        }
    }

    ToolButton {
        id: indicatorButton
        anchors.centerIn: parent
        implicitWidth: 32
        implicitHeight: root.regularH
        onClicked: {
            if (opMenu.opened) {
                opMenu.close()
            } else {
                root.popupHint = true
                popupHintTimer.restart()
                Qt.callLater(function() { root.openOpMenu() })
            }
        }
        contentItem: Item {
            anchors.fill: parent
            Canvas {
                id: progressRing
                anchors.centerIn: parent
                width: 20
                height: 20
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var cx = width / 2
                    var cy = height / 2
                    var r = Math.min(width, height) / 2 - 1.5
                    ctx.lineWidth = 2.5
                    ctx.strokeStyle = Theme.color("batchProgressStroke")
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, Math.PI * 2, false)
                    ctx.stroke()

                    var progress = root.displayProgressValue()
                    ctx.strokeStyle = Theme.color("accent")
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, -Math.PI / 2, -Math.PI / 2 + (Math.PI * 2 * progress), false)
                    ctx.stroke()
                }
            }
        }
        background: Rectangle {
            radius: Theme.radiusControl
            color: indicatorButton.hovered ? Theme.color("accentSoft") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: indicatorButton.hovered ? Theme.color("panelBorder") : "transparent"
        }
    }

    Popup {
        id: opMenu
        parent: Overlay.overlay
        modal: false
        focus: false
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        padding: 0
        y: root.height + root.spaceXs
        x: Math.round((indicatorButton.width - width) * 0.5)
        width: 360
        height: popupLayout.implicitHeight + (root.spaceLg + root.spaceSm)
        onOpened: root.popupHint = false
        onClosed: {
            root.popupHint = false
            cancelDialog.close()
        }
        onWidthChanged: {
            if (opened) {
                root.positionOpMenu()
            }
        }
        onHeightChanged: {
            if (opened) {
                root.positionOpMenu()
            }
        }

        background: DSStyle.PopupSurface {
            popupRadius: Theme.radiusCard
            popupOpacity: Theme.opacitySurfaceStrong
        }

        contentItem: ColumnLayout {
            id: popupLayout
            spacing: root.spaceXs

            Label {
                Layout.leftMargin: root.spaceMd
                Layout.rightMargin: root.spaceMd
                Layout.topMargin: root.spaceMd
                Layout.fillWidth: true
                text: {
                    var state = root.fileManagerApi ? String(root.fileManagerApi.batchOperationState || "") : ""
                    if (state === "preparing") return "Preparing operation..."
                    if (state === "cancelling") return "Cancelling operation..."
                    var op = root.displayOperationType()
                    var paused = (root.fileManagerApi && root.fileManagerApi.batchOperationPaused) ? " (paused)" : ""
                    if (op === "copy") return "Copy in progress" + paused
                    if (op === "move") return "Move in progress" + paused
                    if (op === "delete") return "Delete in progress" + paused
                    if (op === "trash") return "Move to Trash in progress" + paused
                    if (op === "restore") return "Restore in progress" + paused
                    return "Operation in progress"
                }
                color: Theme.color("textPrimary")
                font.weight: Theme.fontWeight("bold")
                elide: Text.ElideRight
            }

            Label {
                Layout.leftMargin: root.spaceMd
                Layout.rightMargin: root.spaceMd
                Layout.fillWidth: true
                text: {
                    var state = root.fileManagerApi ? String(root.fileManagerApi.batchOperationState || "") : ""
                    if (state === "preparing") return "Scanning files and checking conflicts..."
                    var total = root.displayTotalValue()
                    var current = root.displayCurrentValue()
                    var large = total > (8 * 1024 * 1024)
                    if (large) {
                        return humanBytes(current) + " / " + humanBytes(total) + "  (" + percentText() + ")"
                    }
                    return String(current) + " / " + String(total) + "  (" + percentText() + ")"
                }
                color: Theme.color("textSecondary")
                elide: Text.ElideRight
            }

            Label {
                Layout.leftMargin: root.spaceMd
                Layout.rightMargin: root.spaceMd
                Layout.fillWidth: true
                text: root.etaSeconds >= 0 ? ("ETA: " + humanDuration(root.etaSeconds)) : "ETA: calculating..."
                color: Theme.color("textSecondary")
            }

            Label {
                Layout.leftMargin: root.spaceMd
                Layout.rightMargin: root.spaceMd
                Layout.fillWidth: true
                visible: root.fileManagerApi && Number(root.fileManagerApi.batchOperationTotal || 0) > (8 * 1024 * 1024)
                text: root.speedBytesPerSec > 0 ? ("Speed: " + humanBytes(root.speedBytesPerSec) + "/s") : "Speed: --"
                color: Theme.color("textSecondary")
            }

            ProgressBar {
                Layout.leftMargin: root.spaceMd
                Layout.rightMargin: root.spaceMd
                Layout.fillWidth: true
                from: 0
                to: 1
                value: root.displayProgressValue()
                indeterminate: root.fileManagerApi && String(root.fileManagerApi.batchOperationState || "") === "preparing"
            }

            Rectangle {
                Layout.leftMargin: root.spaceSm
                Layout.rightMargin: root.spaceSm
                Layout.fillWidth: true
                height: 1
                color: Theme.color("menuBorder")
            }

            RowLayout {
                Layout.leftMargin: root.spaceMd
                Layout.rightMargin: root.spaceMd
                Layout.fillWidth: true
                spacing: root.spaceSm

                Button {
                    text: (root.fileManagerApi && root.fileManagerApi.batchOperationPaused) ? "Resume" : "Pause"
                    onClicked: {
                        if (!root.fileManagerApi) {
                            return
                        }
                        if (root.fileManagerApi.batchOperationPaused && root.fileManagerApi.resumeActiveBatchOperation) {
                            root.fileManagerApi.resumeActiveBatchOperation()
                        } else if (!root.fileManagerApi.batchOperationPaused && root.fileManagerApi.pauseActiveBatchOperation) {
                            root.fileManagerApi.pauseActiveBatchOperation()
                        }
                    }
                }

                Button {
                    text: "Cancel..."
                    onClicked: cancelDialog.open()
                }
            }

            Rectangle {
                Layout.leftMargin: root.spaceSm
                Layout.rightMargin: root.spaceSm
                Layout.fillWidth: true
                height: 1
                color: Theme.color("menuBorder")
            }

            Button {
                Layout.leftMargin: root.spaceMd
                Layout.rightMargin: root.spaceMd
                Layout.bottomMargin: root.spaceMd
                Layout.alignment: Qt.AlignLeft
                text: "Failed items: " + String(root.failedItems ? root.failedItems.length : 0)
                enabled: (root.failedItems && root.failedItems.length > 0)
                onClicked: failedDialog.open()
            }
        }
    }

    Dialog {
        id: cancelDialog
        modal: true
        title: "Cancel Operation"
        standardButtons: Dialog.NoButton
        anchors.centerIn: Overlay.overlay
        width: 380

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.spaceSm
            spacing: root.spaceSm
            Label {
                text: "Choose how to cancel:"
                color: Theme.color("textPrimary")
            }
            Button {
                text: "Stop & keep completed files"
                Layout.fillWidth: true
                onClicked: {
                    if (root.fileManagerApi && root.fileManagerApi.cancelActiveBatchOperation) {
                        root.fileManagerApi.cancelActiveBatchOperation("keep-completed")
                    }
                    cancelDialog.close()
                    opMenu.close()
                }
            }
            Button {
                text: "Stop & remove files created by this operation"
                Layout.fillWidth: true
                onClicked: {
                    if (root.fileManagerApi && root.fileManagerApi.cancelActiveBatchOperation) {
                        root.fileManagerApi.cancelActiveBatchOperation("remove-created")
                    }
                    cancelDialog.close()
                    opMenu.close()
                }
            }
            Button {
                text: "Stop & move created files to trash"
                Layout.fillWidth: true
                onClicked: {
                    if (root.fileManagerApi && root.fileManagerApi.cancelActiveBatchOperation) {
                        root.fileManagerApi.cancelActiveBatchOperation("trash-created")
                    }
                    cancelDialog.close()
                    opMenu.close()
                }
            }
        }
    }

    Dialog {
        id: failedDialog
        modal: true
        title: "Failed Items"
        standardButtons: Dialog.NoButton
        anchors.centerIn: Overlay.overlay
        width: 560
        height: 420

        onOpened: root.refreshFailedItems()

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.spaceSm
            spacing: root.spaceSm

            Label {
                text: "Failed entries: " + String(root.failedItems ? root.failedItems.length : 0)
                color: Theme.color("textPrimary")
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.failedItems
                spacing: Theme.metric("spacingXxs")
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 64
                    radius: Theme.radiusControl
                    color: Theme.color("batchFailedItemBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("batchFailedItemBorder")

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: root.spaceSm
                        spacing: root.spaceSm

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.metric("spacingXxs")
                            Label {
                                Layout.fillWidth: true
                                text: String(modelData.path || "")
                                color: Theme.color("textPrimary")
                                elide: Text.ElideMiddle
                            }
                            Label {
                                Layout.fillWidth: true
                                text: String(modelData.error || "failed")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideRight
                            }
                        }

                        Button {
                            text: "Retry"
                            onClicked: {
                                if (root.fileManagerApi && root.fileManagerApi.retryFailedBatchItem) {
                                    root.fileManagerApi.retryFailedBatchItem(index)
                                }
                                root.refreshFailedItems()
                                failedDialog.close()
                                opMenu.close()
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Retry All"
                    enabled: root.failedItems && root.failedItems.length > 0
                    onClicked: {
                        if (root.fileManagerApi && root.fileManagerApi.retryFailedBatchItems) {
                            root.fileManagerApi.retryFailedBatchItems()
                        }
                        root.refreshFailedItems()
                        failedDialog.close()
                        opMenu.close()
                    }
                }
                Button {
                    text: "Close"
                    onClicked: failedDialog.close()
                }
            }
        }
    }

    Component.onCompleted: {
        root.captureProgressSnapshot("", 0, 0)
        refreshFailedItems()
    }
}
