import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Item {
    id: root
    anchors.fill: parent

    property string highlightSettingId: ""
    property var componentIssues: []
    property bool hasBlockingIssues: false
    property int selectedPrinterIndex: -1
    property int _paperSizeRev: 0

    readonly property var paperSizeOptions: ["A4", "A3", "A5", "Letter", "Legal", "Tabloid", "B4", "B5"]
    readonly property var printerAdmin: (typeof PrinterAdmin !== "undefined") ? PrinterAdmin : null
    readonly property string currentFallbackId: root.fallbackId()

    function refreshComponentIssues() {
        if (typeof ComponentHealth === "undefined" || !ComponentHealth) {
            componentIssues = []
            hasBlockingIssues = false
            return
        }
        componentIssues = ComponentHealth.missingComponentsForDomain("printing")
        if (ComponentHealth.hasBlockingMissingForDomain) {
            hasBlockingIssues = !!ComponentHealth.hasBlockingMissingForDomain("printing")
        } else {
            hasBlockingIssues = (componentIssues || []).some(function(issue) {
                return String((issue || {}).severity || "required").toLowerCase() === "required"
            })
        }
    }

    function printerList() {
        return (typeof PrintManager !== "undefined" && PrintManager && PrintManager.printers)
               ? PrintManager.printers : []
    }

    function selectedPrinter() {
        var list = root.printerList()
        if (root.selectedPrinterIndex >= 0 && root.selectedPrinterIndex < list.length)
            return list[root.selectedPrinterIndex]
        return null
    }

    function fallbackId() {
        return (typeof DesktopSettings !== "undefined" && DesktopSettings)
               ? String(DesktopSettings.printPdfFallbackPrinterId || "") : ""
    }

    function setFallback(id) {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return
        DesktopSettings.setPrintPdfFallbackPrinterId(id)
    }

    function clearFallback() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return
        DesktopSettings.setPrintPdfFallbackPrinterId("")
    }

    function doReload() {
        if (typeof PrintManager !== "undefined" && PrintManager && PrintManager.reload)
            PrintManager.reload()
    }

    function systemDefaultPrinterId() {
        var list = root.printerList()
        for (var i = 0; i < list.length; ++i) {
            if (Boolean(list[i].isDefault)) return String(list[i].id || "")
        }
        return ""
    }

    function defaultPrinterComboIndex() {
        var list = root.printerList()
        var def = root.systemDefaultPrinterId()
        if (!def) return 0
        for (var i = 0; i < list.length; ++i) {
            if (String(list[i].id || "") === def) return i + 1
        }
        return 0
    }

    function defaultPaperSize() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return "A4"
        return String(DesktopSettings.settingValue("print.defaultPaperSize", "A4") || "A4")
    }

    function setDefaultPaperSize(size) {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return
        DesktopSettings.setSettingValue("print.defaultPaperSize", String(size || "A4"))
    }

    function paperSizeComboIndex() {
        var _dummy = root._paperSizeRev
        var idx = root.paperSizeOptions.indexOf(root.defaultPaperSize())
        return idx >= 0 ? idx : 0
    }

    Component.onCompleted: root.refreshComponentIssues()

    Connections {
        target: root.printerAdmin
        function onPrinterRemoved(success, printerId, error) {
            if (success) {
                root.selectedPrinterIndex = -1
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

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onPrintPdfFallbackPrinterIdChanged() { /* currentFallbackId re-reads via binding */ }
        function onSettingChanged(path) {
            if (String(path || "") === "print.defaultPaperSize") root._paperSizeRev++
        }
    }

    Connections {
        target: (typeof PrintManager !== "undefined") ? PrintManager : null
        function onPrintersChanged() {
            var count = root.printerList().length
            if (root.selectedPrinterIndex >= count)
                root.selectedPrinterIndex = count > 0 ? count - 1 : -1
        }
    }

    // ── Add Printer dialog ────────────────────────────────────────────────

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

            Rectangle {
                visible: addPrinterSheet.errorText.length > 0
                Layout.fillWidth: true
                radius: Theme.radiusMd
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

            ColumnLayout {
                spacing: 4; Layout.fillWidth: true
                Label { text: qsTr("Queue name"); font.pixelSize: Theme.fontSize("small"); color: Theme.color("textSecondary") }
                TextField { id: addNameField; Layout.fillWidth: true; placeholderText: qsTr("e.g. Office_Printer"); font.pixelSize: Theme.fontSize("regular") }
            }

            ColumnLayout {
                spacing: 4; Layout.fillWidth: true
                Label { text: qsTr("Device address (URI)"); font.pixelSize: Theme.fontSize("small"); color: Theme.color("textSecondary") }
                TextField { id: addUriField; Layout.fillWidth: true; placeholderText: qsTr("ipp://192.168.1.100/ipp/print"); font.pixelSize: Theme.fontSize("regular") }
            }

            ColumnLayout {
                spacing: 4; Layout.fillWidth: true
                visible: (root.printerAdmin && root.printerAdmin.discoveredDevices.length > 0)
                         || (root.printerAdmin && root.printerAdmin.busy)
                Label { text: qsTr("Discovered devices"); font.pixelSize: Theme.fontSize("small"); color: Theme.color("textSecondary") }
                BusyIndicator { visible: root.printerAdmin ? root.printerAdmin.busy : false; running: visible; implicitWidth: 24; implicitHeight: 24 }
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

            ColumnLayout {
                spacing: 4; Layout.fillWidth: true
                Label { text: qsTr("Driver (optional — leave blank for generic driver)"); font.pixelSize: Theme.fontSize("small"); color: Theme.color("textSecondary") }
                TextField { id: addPpdField; Layout.fillWidth: true; placeholderText: qsTr("everywhere  or  /path/to/printer.ppd"); font.pixelSize: Theme.fontSize("regular") }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Button {
                    text: root.printerAdmin && root.printerAdmin.busy ? qsTr("Discovering…") : qsTr("Discover Devices")
                    enabled: root.printerAdmin !== null && !root.printerAdmin.busy && !root.hasBlockingIssues
                    font.pixelSize: Theme.fontSize("small")
                    onClicked: root.printerAdmin.discoverDevices()
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Add Printer"); highlighted: true
                    enabled: addNameField.text.trim().length > 0 && addUriField.text.trim().length > 0 && !root.hasBlockingIssues
                    font.pixelSize: Theme.fontSize("small")
                    onClicked: {
                        if (root.printerAdmin) {
                            addPrinterSheet.errorText = ""
                            root.printerAdmin.addPrinter(addNameField.text.trim(), addUriField.text.trim(), addPpdField.text.trim())
                        }
                    }
                }
            }
        }
    }

    // ── Page layout ───────────────────────────────────────────────────────

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        MissingComponentsCard {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            issues: root.componentIssues
            visible: (root.componentIssues || []).length > 0
            message: qsTr("Printing component missing. Some printer features are unavailable.")
            installHandler: function(componentId) { return ComponentHealth.installComponent(componentId) }
            onRefreshRequested: root.refreshComponentIssues()
            onPostInstall: root.doReload()
        }

        Rectangle {
            id: errorBanner
            visible: false
            property string message: ""
            Layout.fillWidth: true
            Layout.topMargin: visible ? 8 : 0
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            radius: Theme.radiusControl
            color: Theme.color("errorSoft")
            implicitHeight: visible ? errRow.implicitHeight + 16 : 0
            RowLayout {
                id: errRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
                spacing: 8
                Text { Layout.fillWidth: true; text: errorBanner.message; color: Theme.color("error"); font.pixelSize: Theme.fontSize("small"); wrapMode: Text.WordWrap }
                ToolButton { text: "×"; font.pixelSize: Theme.fontSize("regular"); onClicked: errorBanner.visible = false }
            }
        }

        // Two-pane card
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 12
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            Rectangle {
                id: card
                anchors.fill: parent
                radius: Theme.radiusCard
                color: Theme.color("surface")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("panelBorder")
                clip: true

                // Left panel
                Item {
                    id: leftPanel
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 210

                    ListView {
                        id: printerListView
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: listToolbar.top
                        model: root.printerList()
                        currentIndex: root.selectedPrinterIndex
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index
                            readonly property bool isPrinterDefault: Boolean(modelData.isDefault)
                            readonly property bool isPrinterAvailable: Boolean(modelData.isAvailable !== false)

                            id: printerDelegate
                            width: printerListView.width
                            height: 52
                            padding: 0
                            highlighted: printerListView.currentIndex === index
                            onClicked: root.selectedPrinterIndex = index

                            background: Rectangle {
                                color: printerDelegate.highlighted
                                       ? Theme.color("accentSoft")
                                       : printerDelegate.hovered ? Theme.color("controlBgHover") : "transparent"
                            }

                            contentItem: Item {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    id: delegateName
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.topMargin: 6
                                    text: String(modelData.id || "")
                                    font.pixelSize: Theme.fontSize("small")
                                    font.weight: Theme.fontWeight("medium")
                                    color: Theme.color("textPrimary")
                                    elide: Text.ElideRight
                                }

                                Row {
                                    anchors.left: parent.left
                                    anchors.top: delegateName.bottom
                                    anchors.topMargin: 2
                                    spacing: 4

                                    Rectangle {
                                        width: 6; height: 6
                                        radius: Theme.radiusSm
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: printerDelegate.isPrinterAvailable ? Theme.color("success") : Theme.color("error")
                                    }

                                    Text {
                                        text: printerDelegate.isPrinterDefault ? qsTr("Default")
                                              : printerDelegate.isPrinterAvailable ? qsTr("Ready") : qsTr("Unavailable")
                                        font.pixelSize: Theme.fontSize("xs")
                                        color: printerDelegate.isPrinterDefault ? Theme.color("accent") : Theme.color("textSecondary")
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: listToolbar
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 32
                        color: Theme.color("surface")

                        Rectangle {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: Theme.borderWidthThin
                            color: Theme.color("panelBorder")
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom

                            ToolButton {
                                width: 32; height: 32
                                text: "+"
                                font.pixelSize: Theme.fontSize("body")
                                enabled: !root.hasBlockingIssues
                                ToolTip.text: qsTr("Add Printer")
                                ToolTip.visible: hovered
                                ToolTip.delay: 600
                                onClicked: addPrinterSheet.open()
                            }

                            Rectangle {
                                width: Theme.borderWidthThin
                                height: parent.height
                                color: Theme.color("panelBorder")
                            }

                            ToolButton {
                                width: 32; height: 32
                                text: "−"
                                font.pixelSize: Theme.fontSize("body")
                                enabled: root.selectedPrinterIndex >= 0 && root.printerAdmin !== null && !root.hasBlockingIssues
                                ToolTip.text: qsTr("Remove Printer")
                                ToolTip.visible: hovered
                                ToolTip.delay: 600
                                onClicked: {
                                    var p = root.selectedPrinter()
                                    if (p && root.printerAdmin)
                                        root.printerAdmin.removePrinter(String(p.id || ""))
                                }
                            }
                        }
                    }
                }

                // Divider
                Rectangle {
                    id: paneDivider
                    anchors.left: leftPanel.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Theme.borderWidthThin
                    color: Theme.color("panelBorder")
                }

                // Right panel
                Rectangle {
                    anchors.left: paneDivider.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    color: Theme.color("surfaceAlt")

                    // Empty state
                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        visible: root.printerList().length === 0 || root.selectedPrinterIndex < 0

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.printerList().length === 0 ? qsTr("No printers are available.")
                                  : qsTr("Select a printer to view its details.")
                            font.pixelSize: Theme.fontSize("body")
                            color: Theme.color("textSecondary")
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: root.printerList().length === 0
                            text: qsTr("Click Add (+) to set up a printer.")
                            font.pixelSize: Theme.fontSize("body")
                            color: Theme.color("textSecondary")
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Detail pane
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 12
                        visible: root.selectedPrinterIndex >= 0 && root.printerList().length > 0

                        readonly property var printer: root.selectedPrinter()

                        Text {
                            Layout.fillWidth: true
                            text: parent.printer ? String(parent.printer.id || "") : ""
                            font.pixelSize: Theme.fontSize("h3")
                            font.weight: Theme.fontWeight("semibold")
                            color: Theme.color("textPrimary")
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            spacing: 6
                            readonly property bool available: parent.printer ? Boolean(parent.printer.isAvailable !== false) : false
                            readonly property bool isDefault: parent.printer ? Boolean(parent.printer.isDefault) : false

                            Rectangle {
                                width: 8; height: 8
                                radius: Theme.radiusSm
                                color: parent.available ? Theme.color("success") : Theme.color("error")
                            }

                            Text {
                                text: parent.available ? qsTr("Ready") : qsTr("Unavailable")
                                font.pixelSize: Theme.fontSize("small")
                                color: Theme.color("textSecondary")
                            }

                            Rectangle {
                                visible: parent.isDefault
                                radius: Theme.radiusSm
                                color: Theme.color("accentSoft")
                                implicitWidth: defaultLabel.implicitWidth + 10
                                implicitHeight: 20
                                Text {
                                    id: defaultLabel
                                    anchors.centerIn: parent
                                    text: qsTr("System Default")
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("accent")
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: Theme.borderWidthThin
                            color: Theme.color("panelBorder")
                        }

                        RowLayout {
                            spacing: 8
                            readonly property var printer: root.selectedPrinter()

                            Button {
                                visible: !(parent.printer && Boolean(parent.printer.isDefault))
                                text: qsTr("Set as Default")
                                font.pixelSize: Theme.fontSize("small")
                                enabled: root.printerAdmin !== null && !root.hasBlockingIssues
                                onClicked: {
                                    var p = root.selectedPrinter()
                                    if (p && root.printerAdmin)
                                        root.printerAdmin.setDefaultPrinter(String(p.id || ""))
                                }
                            }

                            Button {
                                readonly property var printer: root.selectedPrinter()
                                readonly property bool isFallback: printer
                                    ? (String(printer.id || "") === root.currentFallbackId && root.currentFallbackId.length > 0)
                                    : false
                                text: isFallback ? qsTr("PDF Fallback ✓") : qsTr("Use as PDF Fallback")
                                highlighted: isFallback
                                font.pixelSize: Theme.fontSize("small")
                                enabled: !root.hasBlockingIssues
                                onClicked: {
                                    if (isFallback) root.clearFallback()
                                    else if (printer) root.setFallback(String(printer.id || ""))
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }

        // Bottom bar
        Rectangle {
            Layout.fillWidth: true
            height: 72
            color: Theme.color("windowBg")

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: Theme.borderWidthThin
                color: Theme.color("panelBorder")
            }

            GridLayout {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                columns: 2
                rowSpacing: 8
                columnSpacing: 10

                Text {
                    text: qsTr("Default printer:")
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textPrimary")
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }

                ComboBox {
                    Layout.preferredWidth: 220
                    enabled: !root.hasBlockingIssues
                    model: {
                        var items = [qsTr("Last Printer Used")]
                        var list = root.printerList()
                        for (var i = 0; i < list.length; ++i)
                            items.push(String(list[i].id || ""))
                        return items
                    }
                    currentIndex: root.defaultPrinterComboIndex()
                    onActivated: {
                        if (currentIndex === 0) return
                        var list = root.printerList()
                        var p = list[currentIndex - 1]
                        if (p && root.printerAdmin)
                            root.printerAdmin.setDefaultPrinter(String(p.id || ""))
                    }
                }

                Text {
                    text: qsTr("Default paper size:")
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textPrimary")
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }

                ComboBox {
                    Layout.preferredWidth: 220
                    model: root.paperSizeOptions
                    currentIndex: root.paperSizeComboIndex()
                    onActivated: root.setDefaultPaperSize(root.paperSizeOptions[currentIndex])
                }
            }
        }
    }
}
