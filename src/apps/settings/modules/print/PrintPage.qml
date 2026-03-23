import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.height
    clip: true

    property string highlightSettingId: ""
    property string fallbackPrinterId: ""
    property int selectedPrinterIndex: -1

    function printers() {
        if (typeof PrintManager === "undefined" || !PrintManager || !PrintManager.printers) {
            return []
        }
        return PrintManager.printers
    }

    function printerIndexForId(printerId) {
        var id = String(printerId || "")
        var rows = printers()
        for (var i = 0; i < rows.length; ++i) {
            if (String((rows[i] && rows[i].id) ? rows[i].id : "") === id) {
                return i
            }
        }
        return rows.length > 0 ? 0 : -1
    }

    function readFallbackPrinterId() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            fallbackPrinterId = ""
            selectedPrinterIndex = printerIndexForId("")
            return
        }
        fallbackPrinterId = String(UIPreferences.getPreference("print.pdfFallbackPrinterId", ""))
        selectedPrinterIndex = printerIndexForId(fallbackPrinterId)
    }

    function saveFallbackPrinterId() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.setPreference) {
            return
        }
        UIPreferences.setPreference("print.pdfFallbackPrinterId",
                                    String(fallbackPrinterField.text || "").trim())
        readFallbackPrinterId()
    }

    function saveSelectedPrinterFromList() {
        var rows = printers()
        if (selectedPrinterIndex < 0 || selectedPrinterIndex >= rows.length) {
            return
        }
        var printerId = String((rows[selectedPrinterIndex] && rows[selectedPrinterIndex].id)
                               ? rows[selectedPrinterIndex].id : "").trim()
        if (printerId.length <= 0 || typeof UIPreferences === "undefined"
                || !UIPreferences || !UIPreferences.setPreference) {
            return
        }
        UIPreferences.setPreference("print.pdfFallbackPrinterId", printerId)
        readFallbackPrinterId()
    }

    function resetFallbackPrinterId() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.setPreference) {
            return
        }
        UIPreferences.setPreference("print.pdfFallbackPrinterId", "")
        readFallbackPrinterId()
    }

    function targetCardForSetting(settingId) {
        var sid = String(settingId || "")
        if (sid === "pdf-fallback-printer") {
            return fallbackPrinterCard
        }
        if (sid === "pdf-fallback-reset") {
            return currentValueCard
        }
        return null
    }

    function focusSetting(settingId) {
        var card = targetCardForSetting(settingId)
        if (!card) {
            return
        }
        var point = card.mapToItem(contentColumn, 0, 0)
        var target = Math.max(0, Number(point.y || 0) - 16)
        var maxY = Math.max(0, Number(contentHeight) - Number(height))
        contentY = Math.max(0, Math.min(maxY, target))
    }

    Component.onCompleted: readFallbackPrinterId()
    onHighlightSettingIdChanged: {
        focusSetting(highlightSettingId)
    }

    Connections {
        target: (typeof UIPreferences !== "undefined") ? UIPreferences : null
        function onPreferenceChanged(key, value) {
            var k = String(key || "")
            if (k === "print.pdfFallbackPrinterId" || k === "print/pdfFallbackPrinterId") {
                root.fallbackPrinterId = String(value || "")
            }
        }
    }

    Connections {
        target: (typeof PrintManager !== "undefined") ? PrintManager : null
        function onPrintersChanged() {
            root.selectedPrinterIndex = root.printerIndexForId(root.fallbackPrinterId)
        }
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 24

        Text {
            text: "Print"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: "#111827"
        }

        SettingGroup {
            title: "Defaults"
            Layout.fillWidth: true

            SettingCard {
                id: fallbackPrinterCard
                objectName: "pdf-fallback-printer"
                Layout.fillWidth: true
                label: "PDF Fallback Printer ID"
                description: "Used by Print dialog when fallback mode is enabled."
                highlighted: root.highlightSettingId === "pdf-fallback-printer"

                RowLayout {
                    spacing: 8

                    TextField {
                        id: fallbackPrinterField
                        Layout.preferredWidth: 320
                        placeholderText: "e.g. CUPS-PDF"
                        text: root.fallbackPrinterId
                    }

                    Button {
                        text: "Save"
                        onClicked: root.saveFallbackPrinterId()
                    }

                    Button {
                        text: "Reset"
                        onClicked: root.resetFallbackPrinterId()
                    }
                }
            }

            SettingCard {
                id: systemPrintersCard
                objectName: "system-printers"
                Layout.fillWidth: true
                label: "Choose From System Printers"
                description: "Use detected CUPS printer IDs as fallback source."

                RowLayout {
                    spacing: 8

                    ComboBox {
                        id: printerCombo
                        Layout.preferredWidth: 320
                        model: root.printers()
                        textRole: "name"
                        valueRole: "id"
                        currentIndex: root.selectedPrinterIndex
                        onActivated: {
                            root.selectedPrinterIndex = currentIndex
                        }
                    }

                    Button {
                        text: "Apply"
                        enabled: root.selectedPrinterIndex >= 0
                        onClicked: root.saveSelectedPrinterFromList()
                    }

                    Button {
                        text: "Reload"
                        onClicked: {
                            if (typeof PrintManager !== "undefined"
                                    && PrintManager && PrintManager.reload) {
                                PrintManager.reload()
                            }
                        }
                    }
                }
            }

            SettingCard {
                id: currentValueCard
                objectName: "pdf-fallback-reset"
                Layout.fillWidth: true
                label: "Current Value"
                description: "Current saved printer ID preference."
                highlighted: root.highlightSettingId === "pdf-fallback-reset"

                Text {
                    text: root.fallbackPrinterId.length > 0 ? root.fallbackPrinterId : "Not set"
                    color: "#6b7280"
                    font.pixelSize: 13
                }
            }
        }
    }
}
