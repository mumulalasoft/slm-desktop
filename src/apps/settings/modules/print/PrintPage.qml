import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 48
    clip: true

    property string highlightSettingId: ""
    property var componentIssues: []
    property bool installBusy: false
    property string installStatus: ""

    readonly property var printerAdmin: (typeof PrinterAdmin !== "undefined") ? PrinterAdmin : null

    function refreshComponentIssues() {
        if (typeof ComponentHealth === "undefined" || !ComponentHealth) {
            componentIssues = []
            return
        }
        componentIssues = ComponentHealth.missingComponentsForDomain("printing")
    }

    // ── Helpers ───────────────────────────────────────────────────────────
    function printerList() {
        return (typeof PrintManager !== "undefined" && PrintManager && PrintManager.printers)
               ? PrintManager.printers : []
    }

    function fallbackId() {
        return (typeof UIPreferences !== "undefined" && UIPreferences)
               ? String(UIPreferences.getPreference("print.pdfFallbackPrinterId", ""))
               : ""
    }

    function setFallback(id) {
        if (typeof UIPreferences === "undefined" || !UIPreferences) return
        UIPreferences.setPreference("print.pdfFallbackPrinterId", id)
    }

    function clearFallback() {
        if (typeof UIPreferences === "undefined" || !UIPreferences) return
        UIPreferences.setPreference("print.pdfFallbackPrinterId", "")
    }

    function doReload() {
        if (typeof PrintManager !== "undefined" && PrintManager && PrintManager.reload)
            PrintManager.reload()
    }

    // ── Admin result handlers ─────────────────────────────────────────────
    Connections {
        target: root.printerAdmin
        function onPrinterRemoved(success, printerId, error) {
            if (success) {
                root.doReload()
            } else {
                errorBanner.message = qsTr("Could not remove \"%1\": %2").arg(printerId).arg(error)
                errorBanner.visible = true
            }
        }
        function onDefaultPrinterSet(success, printerId, error) {
            if (success) {
                root.doReload()
            } else {
                errorBanner.message = qsTr("Could not set default: %1").arg(error)
                errorBanner.visible = true
            }
        }
        function onPrinterAdded(success, name, error) {
            if (success) {
                addPrinterSheet.close()
                root.doReload()
            } else {
                addPrinterSheet.errorText = error
            }
        }
    }

    // Reactive fallback ID — incremented on preference change so bindings re-read
    property int _rev: 0
    readonly property string currentFallbackId: { _rev; return root.fallbackId() }

    // ComboBox selected index (tracks detected printers list)
    property int selectedIndex: -1

    function syncSelectedIndex() {
        var rows = printerList()
        var fid  = root.currentFallbackId
        for (var i = 0; i < rows.length; ++i) {
            if (String(rows[i].id || "") === fid) { selectedIndex = i; return }
        }
        selectedIndex = rows.length > 0 ? 0 : -1
    }

    // ── Reactive wiring ───────────────────────────────────────────────────
    Component.onCompleted: root.syncSelectedIndex()
    Component.onCompleted: root.refreshComponentIssues()

    Connections {
        target: (typeof UIPreferences !== "undefined") ? UIPreferences : null
        function onPreferenceChanged() { root._rev++; root.syncSelectedIndex() }
    }
    Connections {
        target: (typeof PrintManager !== "undefined") ? PrintManager : null
        function onPrintersChanged() { root.syncSelectedIndex() }
    }

    // ── Deep-link scroll ──────────────────────────────────────────────────
    onHighlightSettingIdChanged: {
        var sid = String(highlightSettingId || "")
        var card = null
        if      (sid === "pdf-fallback-printer") card = fallbackPickerCard
        else if (sid === "pdf-fallback-reset")   card = fallbackCurrentCard
        if (!card) return
        var pt = card.mapToItem(mainLayout, 0, 0)
        var target = Math.max(0, Number(pt.y || 0) - 16)
        contentY = Math.max(0, Math.min(Math.max(0, contentHeight - height), target))
    }

    // ── Add Printer Dialog ────────────────────────────────────────────────
    Dialog {
        id: addPrinterSheet
        title: qsTr("Add Printer")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 64, 480)
        standardButtons: Dialog.Cancel

        property string errorText: ""

        onOpened: {
            addNameField.text = ""
            addUriField.text  = ""
            addPpdField.text  = ""
            addPrinterSheet.errorText = ""
        }

        ColumnLayout {
            width: parent.width
            spacing: 16

            // Error row (shown only on failure)
            Rectangle {
                visible: addPrinterSheet.errorText.length > 0
                Layout.fillWidth: true
                radius: 6
                color: Theme.color("errorSoft")
                implicitHeight: addErrText.implicitHeight + 16

                Text {
                    id: addErrText
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
                    text: addPrinterSheet.errorText
                    color: Theme.color("error")
                    font.pixelSize: Theme.fontSize("small")
                    wrapMode: Text.WordWrap
                }
            }

            // Name
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Label {
                    text: qsTr("Queue name")
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textSecondary")
                }
                TextField {
                    id: addNameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("e.g. Office_Printer")
                    font.pixelSize: Theme.fontSize("regular")
                }
            }

            // Device URI
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Label {
                    text: qsTr("Device address (URI)")
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textSecondary")
                }
                TextField {
                    id: addUriField
                    Layout.fillWidth: true
                    placeholderText: qsTr("ipp://192.168.1.100/ipp/print")
                    font.pixelSize: Theme.fontSize("regular")
                }
            }

            // Discovered devices (populated after Discover)
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                visible: (root.printerAdmin && root.printerAdmin.discoveredDevices.length > 0)
                         || (root.printerAdmin && root.printerAdmin.busy)

                Label {
                    text: qsTr("Discovered devices")
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textSecondary")
                }

                BusyIndicator {
                    visible: root.printerAdmin ? root.printerAdmin.busy : false
                    running: visible
                    implicitWidth: 24; implicitHeight: 24
                }

                Repeater {
                    model: root.printerAdmin ? root.printerAdmin.discoveredDevices : []
                    delegate: ItemDelegate {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.uri
                        font.pixelSize: Theme.fontSize("small")
                        onClicked: addUriField.text = modelData.uri
                    }
                }
            }

            // PPD / Driver (optional)
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Label {
                    text: qsTr("Driver (optional — leave blank for generic IPP)")
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textSecondary")
                }
                TextField {
                    id: addPpdField
                    Layout.fillWidth: true
                    placeholderText: qsTr("everywhere  or  /path/to/printer.ppd")
                    font.pixelSize: Theme.fontSize("regular")
                }
            }

            // Action row
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: root.printerAdmin && root.printerAdmin.busy
                          ? qsTr("Discovering…") : qsTr("Discover Devices")
                    enabled: root.printerAdmin !== null && !root.printerAdmin.busy
                    font.pixelSize: Theme.fontSize("small")
                    onClicked: root.printerAdmin.discoverDevices()
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Add Printer")
                    highlighted: true
                    enabled: addNameField.text.trim().length > 0 && addUriField.text.trim().length > 0
                    font.pixelSize: Theme.fontSize("small")
                    onClicked: {
                        if (root.printerAdmin) {
                            addPrinterSheet.errorText = ""
                            root.printerAdmin.addPrinter(
                                addNameField.text.trim(),
                                addUriField.text.trim(),
                                addPpdField.text.trim()
                            )
                        }
                    }
                }
            }
        }
    }

    // ── Layout ────────────────────────────────────────────────────────────
    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 24

        // ── Error banner (admin operation failures) ───────────────────────
        Rectangle {
            id: errorBanner
            visible: false
            property string message: ""
            Layout.fillWidth: true
            radius: 8
            color: Theme.color("errorSoft")
            implicitHeight: errRow.implicitHeight + 16

            RowLayout {
                id: errRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: errorBanner.message
                    color: Theme.color("error")
                    font.pixelSize: Theme.fontSize("small")
                    wrapMode: Text.WordWrap
                }
                ToolButton {
                    text: "×"
                    font.pixelSize: Theme.fontSize("regular")
                    onClicked: errorBanner.visible = false
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: (root.componentIssues || []).length > 0
            radius: 8
            color: Theme.color("warningBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("warning")
            implicitHeight: depWarnCol.implicitHeight + 16

            ColumnLayout {
                id: depWarnCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Printing component missing. Some printer features are unavailable.")
                    color: Theme.color("textPrimary")
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: root.componentIssues || []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: String((modelData || {}).title || (modelData || {}).componentId || "Unknown")
                                  + " — "
                                  + String((modelData || {}).guidance || (modelData || {}).description || "")
                            color: Theme.color("textSecondary")
                            wrapMode: Text.WordWrap
                        }
                        Button {
                            visible: !!(modelData || {}).autoInstallable
                            enabled: !root.installBusy
                            text: root.installBusy ? qsTr("Installing...") : qsTr("Install")
                            onClicked: {
                                root.installBusy = true
                                var res = ComponentHealth.installComponent(String((modelData || {}).componentId || ""))
                                root.installBusy = false
                                root.installStatus = (!!res && !!res.ok)
                                        ? qsTr("Install completed. Rechecking...")
                                        : (qsTr("Install failed: ") + String((res && res.error) ? res.error : "unknown"))
                                root.refreshComponentIssues()
                                root.doReload()
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.installStatus.length > 0
                    text: root.installStatus
                    color: Theme.color("textSecondary")
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ── Printers ──────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Printers")
            Layout.fillWidth: true

            // One SettingCard per detected printer
            Repeater {
                model: root.printerList()

                delegate: SettingCard {
                    id: printerCard
                    required property var modelData
                    required property int index

                    readonly property string pid:        String(modelData.id          || "")
                    readonly property bool isSysDefault: Boolean(modelData.isDefault)
                    readonly property bool available:    Boolean(modelData.isAvailable !== false)
                    readonly property bool isFallback:   pid === root.currentFallbackId && pid.length > 0
                    property bool confirmingRemove:      false

                    label: pid
                    description: available ? qsTr("Ready") : qsTr("Unavailable")
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: 8

                        // Availability dot
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: available ? Theme.color("success") : Theme.color("error")
                        }

                        // System default badge
                        Rectangle {
                            visible: isSysDefault
                            radius: 4
                            color: Theme.color("accentSoft")
                            implicitWidth: sysDefaultLabel.implicitWidth + 10
                            implicitHeight: 22

                            Text {
                                id: sysDefaultLabel
                                anchors.centerIn: parent
                                text: qsTr("Default")
                                font.pixelSize: Theme.fontSize("xs")
                                color: Theme.color("accent")
                            }
                        }

                        // Set as default button
                        Button {
                            visible: !isSysDefault
                            text: qsTr("Set Default")
                            font.pixelSize: Theme.fontSize("small")
                            enabled: root.printerAdmin !== null
                            onClicked: {
                                if (root.printerAdmin)
                                    root.printerAdmin.setDefaultPrinter(pid)
                            }
                        }

                        // Set / clear fallback button
                        Button {
                            text: isFallback ? qsTr("PDF Fallback ✓") : qsTr("Set PDF Fallback")
                            highlighted: isFallback
                            font.pixelSize: Theme.fontSize("small")
                            onClicked: {
                                if (isFallback) root.clearFallback()
                                else root.setFallback(pid)
                            }
                        }

                        // Remove button (with inline confirmation)
                        Button {
                            text: printerCard.confirmingRemove ? qsTr("Confirm Remove") : qsTr("Remove")
                            font.pixelSize: Theme.fontSize("small")
                            enabled: root.printerAdmin !== null
                            palette.buttonText: printerCard.confirmingRemove
                                                ? Theme.color("error") : undefined
                            onClicked: {
                                if (!printerCard.confirmingRemove) {
                                    printerCard.confirmingRemove = true
                                    confirmResetTimer.restart()
                                } else {
                                    printerCard.confirmingRemove = false
                                    if (root.printerAdmin)
                                        root.printerAdmin.removePrinter(pid)
                                }
                            }

                            Timer {
                                id: confirmResetTimer
                                interval: 4000
                                repeat: false
                                onTriggered: printerCard.confirmingRemove = false
                            }
                        }
                    }
                }
            }

            // Empty state
            Label {
                visible: root.printerList().length === 0
                text: qsTr("No printers found. Check that your printer is connected.")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 4
            }

            // Bottom row: Reload + Add Printer
            RowLayout {
                Layout.fillWidth: true
                Layout.rightMargin: 4
                spacing: 8

                Button {
                    text: qsTr("Add Printer…")
                    font.pixelSize: Theme.fontSize("small")
                    onClicked: addPrinterSheet.open()
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Reload")
                    font.pixelSize: Theme.fontSize("small")
                    onClicked: root.doReload()
                }
            }
        }

        // ── PDF Fallback ───────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("PDF Fallback")
            Layout.fillWidth: true

            // Picker row
            SettingCard {
                id: fallbackPickerCard
                objectName: "pdf-fallback-printer"
                label: qsTr("Fallback Printer")
                description: qsTr("Used by the Print dialog when direct PDF output is unavailable.")
                Layout.fillWidth: true
                highlighted: root.highlightSettingId === "pdf-fallback-printer"

                RowLayout {
                    spacing: 8

                    ComboBox {
                        id: fallbackCombo
                        Layout.preferredWidth: 240
                        model: root.printerList()
                        textRole: "name"
                        valueRole: "id"
                        currentIndex: root.selectedIndex
                        displayText: {
                            var rows = root.printerList()
                            if (root.selectedIndex >= 0 && root.selectedIndex < rows.length)
                                return String(rows[root.selectedIndex].name || rows[root.selectedIndex].id || "")
                            return root.currentFallbackId.length > 0
                                   ? root.currentFallbackId : qsTr("(none)")
                        }
                        onActivated: root.selectedIndex = currentIndex
                    }

                    Button {
                        text: qsTr("Apply")
                        enabled: root.selectedIndex >= 0
                        onClicked: {
                            var rows = root.printerList()
                            if (root.selectedIndex >= 0 && root.selectedIndex < rows.length)
                                root.setFallback(String(rows[root.selectedIndex].id || ""))
                        }
                    }
                }
            }

            // Current value + clear
            SettingCard {
                id: fallbackCurrentCard
                objectName: "pdf-fallback-reset"
                label: qsTr("Current Selection")
                description: qsTr("The printer ID saved as the PDF fallback.")
                Layout.fillWidth: true
                highlighted: root.highlightSettingId === "pdf-fallback-reset"

                RowLayout {
                    spacing: 8

                    Text {
                        text: root.currentFallbackId.length > 0
                              ? root.currentFallbackId
                              : qsTr("(none)")
                        font.pixelSize: Theme.fontSize("small")
                        font.family: root.currentFallbackId.length > 0 ? "monospace" : Theme.fontFamilyUi
                        color: root.currentFallbackId.length > 0
                               ? Theme.color("textPrimary")
                               : Theme.color("textSecondary")
                    }

                    Button {
                        visible: root.currentFallbackId.length > 0
                        text: qsTr("Clear")
                        font.pixelSize: Theme.fontSize("small")
                        onClicked: root.clearFallback()
                    }
                }
            }
        }
    }
}
