import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle

// PrintJobApplet — topbar indicator for active print jobs.
//
// Visible only when there is at least one tracked job.
// Shows a spinner animation while any job is printing.
// Opens a popup with the current job list, status, progress, and cancel action.
// Fires desktop notifications on job success or failure.
//
// Required context property: PrintDialogController (set via rootContext)
// Required context property: NotificationManager (set via rootContext)

Item {
    id: root

    property var printController: (typeof PrintDialogController !== "undefined")
                                  ? PrintDialogController : null
    property var notificationManager: (typeof NotificationManager !== "undefined")
                                      ? NotificationManager : null
    property var popupHost: null
    property bool popupHint: false
    property double lastMenuCloseMs: 0

    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingXs")

    // ── Job tracking ──────────────────────────────────────────────────────

    // Each entry: { jobHandle, documentTitle, printerName, status, progress, errorDetail }
    // status: "printing" | "complete" | "failed" | "queued"
    ListModel { id: jobModel }

    // Active = jobs that are not yet complete/failed.
    readonly property int activeJobCount: {
        var n = 0
        for (var i = 0; i < jobModel.count; ++i) {
            var s = jobModel.get(i).status
            if (s !== "complete" && s !== "failed") n++
        }
        return n
    }

    readonly property bool anyPrinting: {
        for (var i = 0; i < jobModel.count; ++i) {
            if (jobModel.get(i).status === "printing") return true
        }
        return false
    }

    // Applet is only visible when there's something to show.
    implicitWidth: visible ? indicatorButton.implicitWidth : 0
    implicitHeight: visible ? indicatorButton.implicitHeight : 0
    visible: jobModel.count > 0

    // ── Helpers ───────────────────────────────────────────────────────────

    function iconSource(name) {
        return "image://themeicon/" + name + "?v=" +
               ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                ? ThemeIconController.revision : 0)
    }

    function findJobIndex(handle) {
        for (var i = 0; i < jobModel.count; ++i) {
            if (jobModel.get(i).jobHandle === handle) return i
        }
        return -1
    }

    function addJob(handle, title, printerName) {
        var idx = findJobIndex(handle)
        if (idx >= 0) return
        jobModel.append({
            jobHandle:     handle,
            documentTitle: title.length > 0 ? title : qsTr("Document"),
            printerName:   printerName,
            status:        "queued",
            progress:      0.0,
            errorDetail:   ""
        })
    }

    function updateJobStatus(handle, status, errorDetail) {
        var idx = findJobIndex(handle)
        if (idx < 0) return
        jobModel.setProperty(idx, "status", status)
        if (errorDetail !== undefined)
            jobModel.setProperty(idx, "errorDetail", errorDetail)
    }

    function scheduleRemove(handle) {
        removeTimer.pendingHandle = handle
        removeTimer.restart()
    }

    function sendNotification(summary, body, urgency) {
        if (!root.notificationManager || !root.notificationManager.Notify) return
        // urgency: 0=low, 1=normal, 2=critical
        root.notificationManager.Notify(
            "Print",          // appName
            0,                // replacesId (0 = new)
            "printer-symbolic",
            summary,
            body,
            [],               // actions
            { "urgency": urgency !== undefined ? urgency : 1 },
            urgency === 2 ? 8000 : 5000
        )
    }

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) return
        root.popupHint = true
        popupHintTimer.restart()
        Qt.callLater(function() { menu.open() })
    }

    // ── Timers ────────────────────────────────────────────────────────────

    Timer {
        id: popupHintTimer
        interval: 320
        repeat: false
        onTriggered: root.popupHint = false
    }

    Timer {
        id: removeTimer
        property string pendingHandle: ""
        interval: 6000
        repeat: false
        onTriggered: {
            var idx = root.findJobIndex(pendingHandle)
            if (idx >= 0) {
                jobModel.remove(idx)
            }
        }
    }

    // ── PrintDialogController connections ─────────────────────────────────

    Connections {
        target: root.printController
        enabled: root.printController !== null

        function onIsSubmittingChanged() {
            if (!root.printController) return
            if (root.printController.isSubmitting) {
                // A new submission started — create a pending job entry.
                var session = root.printController.printSession
                var title   = (session && session.documentTitle)
                              ? String(session.documentTitle) : ""
                var printer = root.printController.activePrinterId
                root.addJob("pending-" + Date.now(), title, printer)
            }
        }

        function onSubmitSucceeded(jobId) {
            // Replace the last pending entry with the real job handle.
            for (var i = jobModel.count - 1; i >= 0; --i) {
                if (jobModel.get(i).status === "queued"
                        || jobModel.get(i).status === "printing") {
                    jobModel.setProperty(i, "jobHandle", String(jobId))
                    break
                }
            }
            root.updateJobStatus(String(jobId), "printing")
        }

        function onJobStatusUpdated(jobHandle, status, statusDetail) {
            var handle = String(jobHandle)
            var idx = root.findJobIndex(handle)
            if (idx < 0) return

            root.updateJobStatus(handle, status, statusDetail || undefined)

            if (status === "complete") {
                var title   = jobModel.get(idx).documentTitle
                var printer = jobModel.get(idx).printerName
                root.sendNotification(
                    qsTr("Document printed"),
                    printer.length > 0
                        ? qsTr("\u201C%1\u201D sent to %2").arg(title).arg(printer)
                        : qsTr("\u201C%1\u201D sent to printer").arg(title),
                    1   // normal urgency
                )
                root.scheduleRemove(handle)
            } else if (status === "failed") {
                var title2   = jobModel.get(idx).documentTitle
                var printer2 = jobModel.get(idx).printerName
                var detail   = statusDetail || ""
                root.sendNotification(
                    qsTr("Printing failed"),
                    printer2.length > 0
                        ? qsTr("\u201C%1\u201D on %2").arg(title2).arg(printer2)
                        : title2,
                    2   // critical urgency
                )
                root.scheduleRemove(handle)
            }
        }

        function onSubmitFailed(error) {
            // Mark the last queued/printing job as failed.
            for (var i = jobModel.count - 1; i >= 0; --i) {
                var s = jobModel.get(i).status
                if (s === "queued" || s === "printing") {
                    jobModel.setProperty(i, "status", "failed")
                    jobModel.setProperty(i, "errorDetail", String(error))
                    var title = jobModel.get(i).documentTitle
                    var printer = jobModel.get(i).printerName
                    root.sendNotification(
                        qsTr("Printing failed"),
                        printer.length > 0
                            ? qsTr("\u201C%1\u201D on %2").arg(title).arg(printer)
                            : title,
                        2   // critical urgency
                    )
                    root.scheduleRemove(jobModel.get(i).jobHandle)
                    break
                }
            }
        }
    }

    // ── Topbar button ─────────────────────────────────────────────────────

    ToolButton {
        id: indicatorButton
        anchors.fill: parent
        padding: 0
        Accessible.role: Accessible.Button
        Accessible.name: root.activeJobCount > 0
            ? qsTr("%n print job(s) active", "", root.activeJobCount)
            : qsTr("Print jobs")
        Accessible.description: root.anyPrinting
            ? qsTr("Printing in progress")
            : qsTr("No active print jobs")
        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) return
            if (menu.opened || menu.visible) {
                root.lastMenuCloseMs = Date.now()
                menu.close()
                return
            }
            root.openMenuSafely()
        }

        contentItem: Item {
            implicitWidth: root.iconSize
            implicitHeight: root.iconSize

            // Printer icon
            IconImage {
                id: printerIcon
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: root.iconSource("printer-symbolic")
                color: Theme.color("textOnGlass")

                // Subtle pulse while printing
                SequentialAnimation on opacity {
                    running: root.anyPrinting
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.55; duration: 700; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0;  duration: 700; easing.type: Easing.InOutSine }
                }
                opacity: root.anyPrinting ? 1.0 : 1.0
            }

            // Job count badge
            Rectangle {
                visible: root.activeJobCount > 0
                anchors.right: parent.right
                anchors.top: parent.top
                width: 14
                height: 14
                radius: Theme.radiusMdPlus
                color: root.anyPrinting ? Theme.color("accent") : Theme.color("textSecondary")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("menuBg")

                Behavior on color { ColorAnimation { duration: Theme.durationSm } }

                Text {
                    anchors.centerIn: parent
                    text: root.activeJobCount > 9 ? "9+" : String(root.activeJobCount)
                    color: Theme.color("accentText")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("micro")
                    font.weight: Theme.fontWeight("bold")
                }
            }
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: indicatorButton.hovered ? Theme.color("accentSoft") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: indicatorButton.hovered ? Theme.color("panelBorder") : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.durationSm; easing.type: Easing.OutCubic } }
            Behavior on border.color { ColorAnimation { duration: Theme.durationSm; easing.type: Easing.OutCubic } }
        }
    }

    // ── Popup menu ────────────────────────────────────────────────────────

    IndicatorMenu {
        id: menu
        anchorItem: indicatorButton
        popupGap: root.popupGap
        popupWidth: 300

        onAboutToShow: root.popupHint = false
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        // Header
        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: qsTr("Print Jobs")
                emphasized: true
            }
        }

        // Empty state
        MenuItem {
            visible: jobModel.count === 0
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: qsTr("No recent print jobs")
            }
        }

        // Job rows — one MenuItem per job
        Instantiator {
            model: jobModel
            delegate: MenuItem {
                enabled: model.status !== "complete" && model.status !== "failed"
                contentItem: ColumnLayout {
                    spacing: 3
                    clip: true

                    // Title row
                    RowLayout {
                        spacing: Theme.metric("spacingSm")
                        Layout.fillWidth: true

                        // Status dot
                        Rectangle {
                            width: 8; height: 8
                            radius: 4
                            color: {
                                switch (model.status) {
                                case "printing": return Theme.color("accent")
                                case "complete": return Theme.color("success")
                                case "failed":   return Theme.color("warning")
                                default:         return Theme.color("textSecondary")
                                }
                            }
                            Behavior on color { ColorAnimation { duration: 200 } }

                            // Animated pulse for printing state
                            SequentialAnimation on opacity {
                                running: model.status === "printing"
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 500; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutSine }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: model.documentTitle
                            color: Theme.color("textPrimary")
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Theme.fontWeight("semibold")
                            elide: Text.ElideRight
                        }

                        // Cancel button (only for non-terminal states)
                        ToolButton {
                            visible: model.status !== "complete" && model.status !== "failed"
                            padding: 0
                            implicitWidth: 20
                            implicitHeight: 20
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Cancel print job: %1").arg(model.documentTitle)
                            contentItem: Text {
                                text: "\u00D7"   // ×
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                font.weight: Theme.fontWeight("bold")
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Item {}
                            onClicked: {
                                if (root.printController && root.printController.cancelJob) {
                                    root.printController.cancelJob(model.jobHandle)
                                }
                                root.updateJobStatus(model.jobHandle, "failed",
                                                     qsTr("Cancelled"))
                                root.scheduleRemove(model.jobHandle)
                            }
                        }
                    }

                    // Printer name + status
                    Text {
                        Layout.fillWidth: true
                        text: {
                            var printer = model.printerName || ""
                            var status  = ""
                            switch (model.status) {
                            case "queued":   status = qsTr("Waiting\u2026"); break
                            case "printing": status = qsTr("Printing\u2026"); break
                            case "complete": status = qsTr("Complete"); break
                            case "failed":
                                status = model.errorDetail.length > 0
                                    ? model.errorDetail
                                    : qsTr("Failed")
                                break
                            }
                            return printer.length > 0
                                ? printer + "  \u00B7  " + status
                                : status
                        }
                        color: model.status === "failed"
                               ? Theme.color("warning")
                               : Theme.color("textSecondary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                    }

                    // Progress bar (printing only)
                    Rectangle {
                        visible: model.status === "printing"
                        Layout.fillWidth: true
                        height: 3
                        radius: 2
                        color: Theme.color("borderSubtle")

                        Rectangle {
                            width: parent.width * Math.max(0.05, model.progress)
                            height: parent.height
                            radius: parent.radius
                            color: Theme.color("accent")
                            Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                        }
                    }
                }
            }
            onObjectAdded: function(index, object) {
                menu.insertItem(2 + index, object)
            }
            onObjectRemoved: function(index, object) {
                menu.removeItem(object)
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Open Print Settings\u2026")
            onTriggered: {
                if (typeof AppModel !== "undefined" && AppModel
                        && AppModel.launch) {
                    AppModel.launch("org.slm.settings")
                }
            }
        }
    }
}
