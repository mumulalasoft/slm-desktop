import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop
import Style as DSStyle

Window {
    id: dialog

    property var parentWindow: null
    property var printManager: null
    property var printSession: null
    property var printPreviewModel: null
    property var printJobSubmitter: null
    property string documentUri: ""
    property string documentTitle: "Document"
    property bool preferPdfOutput: false
    property bool usePdfFallback: false
    property bool showDetails: false
    property string submitErrorText: ""
    property string submitInfoText: ""
    property string fallbackSelectionSource: ""
    readonly property var uiPreferences: (typeof UIPreferences !== "undefined") ? UIPreferences : null

    signal closeRequested()

    color: Theme.color("menuBg")
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    transientParent: parentWindow
    title: "Print"
    width: parentWindow ? Math.min(920, Math.max(760, parentWindow.width - 240)) : 840
    height: parentWindow ? Math.min(640, Math.max(500, parentWindow.height - 180)) : 560
    x: parentWindow ? parentWindow.x + Math.round((parentWindow.width - width) / 2) : 80
    y: parentWindow ? parentWindow.y + Math.round((parentWindow.height - height) / 2) : 60

    function printerIndexForId(printerId) {
        var list = (printManager && printManager.printers) ? printManager.printers : []
        for (var i = 0; i < list.length; ++i) {
            var p = list[i]
            if (String(p.id || "") === String(printerId || "")) {
                return i
            }
        }
        return list.length > 0 ? 0 : -1
    }

    function printerIdAt(index) {
        var list = (printManager && printManager.printers) ? printManager.printers : []
        if (index < 0 || index >= list.length) {
            return ""
        }
        return String(list[index].id || "")
    }

    function syncPreviewDigest() {
        if (!printPreviewModel || !printSession || !printSession.settingsModel) {
            return
        }
        printPreviewModel.documentUri = documentUri
        printPreviewModel.applySettingsDigest(printSession.settingsModel.serialize())
    }

    function preferredPdfPrinterId() {
        var list = (printManager && printManager.printers) ? printManager.printers : []
        for (var i = 0; i < list.length; ++i) {
            var row = list[i] || ({})
            var id = String(row.id || "").toLowerCase()
            var name = String(row.name || "").toLowerCase()
            if (id.indexOf("pdf") >= 0 || id.indexOf("cups-pdf") >= 0
                    || name.indexOf("pdf") >= 0) {
                return String(row.id || "")
            }
        }
        return ""
    }

    function printerExists(printerId) {
        var id = String(printerId || "")
        if (id.length <= 0) {
            return false
        }
        var list = (printManager && printManager.printers) ? printManager.printers : []
        for (var i = 0; i < list.length; ++i) {
            var row = list[i] || ({})
            if (String((row && row.id) ? row.id : "") === id) {
                return true
            }
        }
        return false
    }

    function rememberPdfFallbackPrinter(printerId) {
        var id = String(printerId || "")
        if (id.length <= 0 || !uiPreferences || !uiPreferences.setPreference) {
            return
        }
        uiPreferences.setPreference("print.pdfFallbackPrinterId", id)
    }

    function resetPdfFallbackPrinterPreference() {
        if (!uiPreferences || !uiPreferences.setPreference) {
            return
        }
        uiPreferences.setPreference("print.pdfFallbackPrinterId", "")
        fallbackSelectionSource = ""
        console.log("[print-fallback] preference-reset")
    }

    function applyPdfFallbackSelection(currentSelectedPrinter) {
        var selectedPrinter = String(currentSelectedPrinter || "")
        if (!printSession || !printSession.settingsModel) {
            return selectedPrinter
        }
        if (!usePdfFallback) {
            fallbackSelectionSource = ""
            return selectedPrinter
        }

        var rememberedPdfPrinter = ""
        if (uiPreferences && uiPreferences.getPreference) {
            rememberedPdfPrinter = String(uiPreferences.getPreference("print.pdfFallbackPrinterId", ""))
        }
        var pdfPrinter = printerExists(rememberedPdfPrinter)
                ? rememberedPdfPrinter : preferredPdfPrinterId()
        if (pdfPrinter.length > 0) {
            selectedPrinter = pdfPrinter
            printSession.settingsModel.printerId = selectedPrinter
            rememberPdfFallbackPrinter(selectedPrinter)
            fallbackSelectionSource = printerExists(rememberedPdfPrinter) ? "preference" : "auto"
            submitInfoText = "Using PDF fallback printer."
            console.log("[print-fallback] enabled source=" + fallbackSelectionSource
                        + " printer=" + selectedPrinter)
            return selectedPrinter
        }

        fallbackSelectionSource = "none"
        if (!selectedPrinter.length) {
            submitInfoText = "No PDF fallback printer found. Select a printer manually."
        }
        console.log("[print-fallback] enabled source=none printer=")
        return selectedPrinter
    }

    function initializeSession() {
        submitErrorText = ""
        submitInfoText = ""
        if (!printSession || !printSession.settingsModel) {
            return
        }
        usePdfFallback = !!preferPdfOutput
        fallbackSelectionSource = ""
        printSession.begin(documentUri, documentTitle)
        var selectedPrinter = String(printSession.settingsModel.printerId || "")
        selectedPrinter = applyPdfFallbackSelection(selectedPrinter)
        if (!selectedPrinter.length && printManager) {
            selectedPrinter = String(printManager.defaultPrinterId || "")
            if (selectedPrinter.length) {
                printSession.settingsModel.printerId = selectedPrinter
            }
        }
        if (selectedPrinter.length && printManager) {
            printSession.printerCapability = printManager.capabilities(selectedPrinter)
        }
        if (printPreviewModel) {
            printPreviewModel.documentUri = documentUri
            printPreviewModel.totalPages = 1
            printPreviewModel.currentPage = 1
            printPreviewModel.errorString = ""
            printPreviewModel.loading = false
        }
        syncPreviewDigest()
    }

    function submitPrint() {
        submitErrorText = ""
        submitInfoText = ""
        if (!printSession || !printJobSubmitter) {
            submitErrorText = "Print backend is unavailable."
            return
        }
        printSession.documentUri = documentUri
        printSession.documentTitle = documentTitle
        var payload = printSession.buildJobPayload()
        if (!payload.success) {
            submitErrorText = String(payload.error || "Unable to build print job payload.")
            return
        }
        var result = printJobSubmitter.submit(payload)
        if (!!result.success) {
            submitInfoText = String(result.jobId || "Job submitted")
            closeRequested()
        } else {
            submitErrorText = String(result.error || "Failed to submit print job.")
        }
    }

    onVisibleChanged: {
        if (visible) {
            requestActivate()
            initializeSession()
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: dialog.visible
        onActivated: dialog.closeRequested()
    }

    Connections {
        target: printSession && printSession.settingsModel ? printSession.settingsModel : null
        function onSettingsChanged() {
            dialog.syncPreviewDigest()
        }
    }

    onUsePdfFallbackChanged: {
        if (!visible || !printSession || !printSession.settingsModel) {
            return
        }
        var selectedPrinter = String(printSession.settingsModel.printerId || "")
        selectedPrinter = applyPdfFallbackSelection(selectedPrinter)
        if (!selectedPrinter.length && printManager) {
            selectedPrinter = String(printManager.defaultPrinterId || "")
            if (selectedPrinter.length) {
                printSession.settingsModel.printerId = selectedPrinter
            }
        }
        if (selectedPrinter.length && printManager) {
            printSession.printerCapability = printManager.capabilities(selectedPrinter)
        }
        syncPreviewDigest()
    }

    Component {
        id: printDialogBody

        RowLayout {
            width: dialog.width - (Theme.metric("spacingMd") * 2)
            spacing: Theme.metric("spacingMd")

            Rectangle {
                Layout.preferredWidth: Math.round(parent.width * 0.42)
                Layout.fillHeight: true
                radius: Theme.radiusControlLarge
                color: Theme.color("fileManagerWindowBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("fileManagerWindowBorder")

                Column {
                    anchors.fill: parent
                    anchors.margins: Theme.metric("spacingSm")
                    spacing: Theme.metric("spacingSm")

                    Row {
                        width: parent.width
                        spacing: Theme.metric("spacingXs")
                        DSStyle.Button {
                            width: 28
                            height: Theme.metric("controlHeightRegular")
                            text: "◀"
                            enabled: printPreviewModel && printPreviewModel.currentPage > 1
                            onClicked: if (printPreviewModel) { printPreviewModel.previousPage() }
                        }
                        DSStyle.Button {
                            width: 28
                            height: Theme.metric("controlHeightRegular")
                            text: "▶"
                            enabled: printPreviewModel && printPreviewModel.currentPage < printPreviewModel.totalPages
                            onClicked: if (printPreviewModel) { printPreviewModel.nextPage() }
                        }
                        DSStyle.Label {
                            text: printPreviewModel
                                  ? ("Page " + printPreviewModel.currentPage + " / " + printPreviewModel.totalPages)
                                  : "Page - / -"
                            color: Theme.color("textSecondary")
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: Math.max(180, parent.height - 42)
                        radius: Theme.radiusControlLarge
                        color: Theme.color("windowBg")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("borderSubtle")

                        Column {
                            anchors.centerIn: parent
                            spacing: Theme.metric("spacingXs")
                            DSStyle.Label {
                                text: "Preview"
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }
                            DSStyle.Label {
                                text: documentTitle
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("title")
                            }
                            DSStyle.Label {
                                text: documentUri
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideMiddle
                                width: Math.min(parent.width, 260)
                                horizontalAlignment: Text.AlignHCenter
                            }
                            DSStyle.Label {
                                visible: !!(printPreviewModel && printPreviewModel.previewCacheKey)
                                text: printPreviewModel ? ("cache: " + printPreviewModel.previewCacheKey) : ""
                                color: Theme.color("textTertiary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideRight
                                width: Math.min(parent.width, 300)
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }
            }

            Column {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.metric("spacingSm")

                Rectangle {
                    visible: dialog.usePdfFallback
                    width: parent.width
                    implicitHeight: fallbackBadgeCol.implicitHeight + Theme.metric("spacingXs")
                    radius: Theme.radiusControlLarge
                    color: Theme.color("accentSoft")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("borderSubtle")

                    Column {
                        id: fallbackBadgeCol
                        anchors.centerIn: parent
                        spacing: 2

                        DSStyle.Label {
                            text: "PDF fallback mode"
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("small")
                        }
                        DSStyle.Label {
                            visible: dialog.fallbackSelectionSource.length > 0
                            text: "Source: " + dialog.fallbackSelectionSource
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.metric("spacingSm")

                    DSStyle.CheckBox {
                        text: "Use PDF fallback printer"
                        checked: dialog.usePdfFallback
                        onToggled: dialog.usePdfFallback = checked
                    }

                    DSStyle.Button {
                        text: "Reset PDF fallback"
                        height: Theme.metric("controlHeightCompact")
                        enabled: dialog.usePdfFallback
                                 && !!(dialog.uiPreferences && dialog.uiPreferences.getPreference
                                       && String(dialog.uiPreferences.getPreference("print.pdfFallbackPrinterId", "")).length > 0)
                        onClicked: {
                            dialog.resetPdfFallbackPrinterPreference()
                            if (dialog.usePdfFallback) {
                                dialog.usePdfFallback = false
                                dialog.usePdfFallback = true
                            }
                        }
                    }
                }

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: Theme.metric("spacingSm")
                    rowSpacing: Theme.metric("spacingSm")

                    DSStyle.Label { text: "Document"; color: Theme.color("textSecondary") }
                    DSStyle.TextField {
                        Layout.fillWidth: true
                        text: dialog.documentUri
                        onTextEdited: dialog.documentUri = text
                    }

                    DSStyle.Label { text: "Printer"; color: Theme.color("textSecondary") }
                    DSStyle.ComboBox {
                        id: printerCombo
                        Layout.fillWidth: true
                        model: printManager ? printManager.printers : []
                        textRole: "name"
                        currentIndex: dialog.printerIndexForId(
                                          (printSession && printSession.settingsModel)
                                          ? printSession.settingsModel.printerId : "")
                        onActivated: {
                            if (!printSession || !printSession.settingsModel) {
                                return
                            }
                            var id = dialog.printerIdAt(currentIndex)
                            printSession.settingsModel.printerId = id
                            if (dialog.usePdfFallback && id.length > 0) {
                                dialog.rememberPdfFallbackPrinter(id)
                                dialog.fallbackSelectionSource = "manual"
                                console.log("[print-fallback] enabled source=manual printer=" + id)
                            }
                            if (id.length && printManager) {
                                printSession.printerCapability = printManager.capabilities(id)
                            }
                        }
                    }

                    DSStyle.Label { text: "Copies"; color: Theme.color("textSecondary") }
                    DSStyle.TextField {
                        Layout.fillWidth: true
                        text: (printSession && printSession.settingsModel)
                              ? String(printSession.settingsModel.copies) : "1"
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: {
                            if (printSession && printSession.settingsModel) {
                                var n = Number(text)
                                printSession.settingsModel.copies = isNaN(n) ? 1 : Math.max(1, Math.floor(n))
                                text = String(printSession.settingsModel.copies)
                            }
                        }
                    }

                    DSStyle.Label { text: "Page Range"; color: Theme.color("textSecondary") }
                    DSStyle.TextField {
                        Layout.fillWidth: true
                        text: (printSession && printSession.settingsModel)
                              ? String(printSession.settingsModel.pageRange) : ""
                        placeholderText: "All"
                        onTextEdited: if (printSession && printSession.settingsModel) {
                                          printSession.settingsModel.pageRange = text
                                      }
                    }

                    DSStyle.Label { text: "Color"; color: Theme.color("textSecondary") }
                    DSStyle.ComboBox {
                        Layout.fillWidth: true
                        model: [
                            { label: "Color", value: "color" },
                            { label: "Black & White", value: "monochrome" }
                        ]
                        textRole: "label"
                        currentIndex: (printSession && printSession.settingsModel
                                       && String(printSession.settingsModel.colorMode) === "monochrome") ? 1 : 0
                        onActivated: if (printSession && printSession.settingsModel) {
                                         printSession.settingsModel.colorMode = model[currentIndex].value
                                     }
                    }

                    DSStyle.Label { text: "Duplex"; color: Theme.color("textSecondary") }
                    DSStyle.ComboBox {
                        Layout.fillWidth: true
                        model: [
                            { label: "Off", value: "one-sided" },
                            { label: "Long Edge", value: "two-sided-long-edge" },
                            { label: "Short Edge", value: "two-sided-short-edge" }
                        ]
                        textRole: "label"
                        currentIndex: {
                            if (!printSession || !printSession.settingsModel) return 0
                            var d = String(printSession.settingsModel.duplex)
                            if (d === "two-sided-long-edge") return 1
                            if (d === "two-sided-short-edge") return 2
                            return 0
                        }
                        onActivated: if (printSession && printSession.settingsModel) {
                                         printSession.settingsModel.duplex = model[currentIndex].value
                                     }
                    }

                    DSStyle.Label { text: "Paper Size"; color: Theme.color("textSecondary") }
                    DSStyle.ComboBox {
                        Layout.fillWidth: true
                        model: (printSession && printSession.printerCapability
                                && printSession.printerCapability.paperSizes)
                               ? printSession.printerCapability.paperSizes : ["A4", "Letter"]
                        currentIndex: {
                            if (!printSession || !printSession.settingsModel) return 0
                            var arr = model
                            var value = String(printSession.settingsModel.paperSize)
                            var i = arr.indexOf(value)
                            return i >= 0 ? i : 0
                        }
                        onActivated: if (printSession && printSession.settingsModel) {
                                         printSession.settingsModel.paperSize = String(model[currentIndex])
                                     }
                    }
                }

                DSStyle.Button {
                    text: dialog.showDetails ? "Hide Details" : "Show Details"
                    onClicked: dialog.showDetails = !dialog.showDetails
                }

                Column {
                    visible: dialog.showDetails
                    spacing: Theme.metric("spacingSm")
                    width: parent.width

                    GridLayout {
                        width: parent.width
                        columns: 2
                        columnSpacing: Theme.metric("spacingSm")
                        rowSpacing: Theme.metric("spacingSm")

                        DSStyle.Label { text: "Quality"; color: Theme.color("textSecondary") }
                        DSStyle.ComboBox {
                            Layout.fillWidth: true
                            model: (printSession && printSession.printerCapability
                                    && printSession.printerCapability.qualityModes)
                                   ? printSession.printerCapability.qualityModes : ["draft", "normal", "high"]
                            currentIndex: {
                                if (!printSession || !printSession.settingsModel) return 0
                                var arr = model
                                var value = String(printSession.settingsModel.quality)
                                var i = arr.indexOf(value)
                                return i >= 0 ? i : 0
                            }
                            onActivated: if (printSession && printSession.settingsModel) {
                                             printSession.settingsModel.quality = String(model[currentIndex])
                                         }
                        }

                        DSStyle.Label { text: "Scale (%)"; color: Theme.color("textSecondary") }
                        DSStyle.TextField {
                            Layout.fillWidth: true
                            text: (printSession && printSession.settingsModel)
                                  ? String(Math.round(Number(printSession.settingsModel.scale))) : "100"
                            inputMethodHints: Qt.ImhDigitsOnly
                            onEditingFinished: if (printSession && printSession.settingsModel) {
                                                   var n = Number(text)
                                                   if (isNaN(n)) n = 100
                                                   printSession.settingsModel.scale = n
                                                   text = String(Math.round(Number(printSession.settingsModel.scale)))
                                               }
                        }

                        DSStyle.Label { text: "Zoom"; color: Theme.color("textSecondary") }
                        DSStyle.ComboBox {
                            Layout.fillWidth: true
                            model: [
                                { label: "Fit Page", value: "fitPage" },
                                { label: "Fit Width", value: "fitWidth" },
                                { label: "Custom", value: "custom" }
                            ]
                            textRole: "label"
                            currentIndex: {
                                if (!printPreviewModel) return 0
                                var mode = String(printPreviewModel.zoomMode)
                                if (mode === "fitWidth") return 1
                                if (mode === "custom") return 2
                                return 0
                            }
                            onActivated: if (printPreviewModel) {
                                             printPreviewModel.zoomMode = model[currentIndex].value
                                         }
                        }
                    }
                }

                DSStyle.Label {
                    visible: submitErrorText.length > 0
                    text: submitErrorText
                    color: Theme.color("warning")
                    font.pixelSize: Theme.fontSize("small")
                    wrapMode: Text.Wrap
                    width: parent.width
                }

                DSStyle.Label {
                    visible: submitInfoText.length > 0
                    text: submitInfoText
                    color: Theme.color("success")
                    font.pixelSize: Theme.fontSize("small")
                    wrapMode: Text.Wrap
                    width: parent.width
                }
            }
        }
    }

    Component {
        id: printDialogFooter

        Row {
            width: dialog.width - (Theme.metric("spacingMd") * 2)
            height: 32
            spacing: Theme.metric("spacingSm")

            Item {
                width: Math.max(0, parent.width - 308)
                height: 1
            }

            DSStyle.Button {
                width: 96
                height: Theme.metric("controlHeightRegular")
                text: "Refresh"
                onClicked: if (printManager && printManager.reload) { printManager.reload() }
            }

            DSStyle.Button {
                width: 96
                height: Theme.metric("controlHeightRegular")
                text: "Cancel"
                onClicked: dialog.closeRequested()
            }
            DSStyle.Button {
                width: 96
                height: Theme.metric("controlHeightRegular")
                text: "Print"
                defaultAction: true
                onClicked: dialog.submitPrint()
            }
        }
    }

    DSStyle.WindowDialogSurface {
        anchors.fill: parent
        title: "Print"
        bodyComponent: printDialogBody
        footerComponent: printDialogFooter
    }
}
