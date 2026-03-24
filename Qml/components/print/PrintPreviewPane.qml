import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style as DSStyle

// Self-contained preview pane for the print dialog.
//
// Required properties:
//   previewModel  — PrintPreviewModel* (C++ object)
//
// Optional:
//   onRenderRequested(dpi, w, h) — called when the pane wants a fresh render.
//   The pane emits renderRequested automatically on geometry changes and when
//   the model's previewChanged() fires.

Rectangle {
    id: root

    required property var previewModel

    signal renderRequested(int dpi, int viewportWidth, int viewportHeight)

    color: Theme.color("windowBg")
    radius: Theme.radiusControlLarge
    border.width: Theme.borderWidthThin
    border.color: Theme.color("borderSubtle")
    clip: true

    // ── Zoom controls ─────────────────────────────────────────────────────
    readonly property int previewDpi: {
        switch (String(previewModel ? previewModel.zoomMode : "fitPage")) {
        case "fitWidth": return 120
        case "custom":   return Math.round(96 * (previewModel ? previewModel.zoomFactor : 1.0))
        default:         return 96  // fitPage
        }
    }

    // Request a render whenever the model says something changed.
    Connections {
        target: root.previewModel
        function onPreviewChanged() { root.scheduleRender() }
    }

    // Also re-render when the pane is resized.
    onWidthChanged:  Qt.callLater(root.scheduleRender)
    onHeightChanged: Qt.callLater(root.scheduleRender)

    Component.onCompleted: root.scheduleRender()

    function scheduleRender() {
        if (!previewModel || previewModel.documentUri === "") return
        const vw = Math.max(32, previewImageArea.width)
        const vh = Math.max(32, previewImageArea.height)
        previewModel.requestRender(root.previewDpi, vw, vh)
        root.renderRequested(root.previewDpi, vw, vh)
    }

    // ── Layout ────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ── Page navigation bar ───────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            DSStyle.Button {
                id: prevBtn
                text: "‹"
                implicitWidth: 28
                implicitHeight: Theme.metric("controlHeightCompact")
                enabled: root.previewModel && root.previewModel.currentPage > 1
                onClicked: {
                    if (root.previewModel) root.previewModel.previousPage()
                }
            }

            DSStyle.Button {
                id: nextBtn
                text: "›"
                implicitWidth: 28
                implicitHeight: Theme.metric("controlHeightCompact")
                enabled: root.previewModel
                         && root.previewModel.currentPage < root.previewModel.totalPages
                onClicked: {
                    if (root.previewModel) root.previewModel.nextPage()
                }
            }

            DSStyle.Label {
                text: root.previewModel
                      ? (root.previewModel.currentPage + " / " + root.previewModel.totalPages)
                      : "— / —"
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                Layout.fillWidth: true
            }

            // Zoom mode selector
            DSStyle.ComboBox {
                id: zoomCombo
                implicitWidth: 96
                implicitHeight: Theme.metric("controlHeightCompact")
                model: ["Fit Page", "Fit Width", "Custom"]
                currentIndex: {
                    if (!root.previewModel) return 0
                    switch (root.previewModel.zoomMode) {
                    case "fitWidth": return 1
                    case "custom":   return 2
                    default:         return 0
                    }
                }
                onActivated: {
                    if (!root.previewModel) return
                    switch (currentIndex) {
                    case 1:  root.previewModel.zoomMode = "fitWidth"; break
                    case 2:  root.previewModel.zoomMode = "custom";   break
                    default: root.previewModel.zoomMode = "fitPage";  break
                    }
                }
            }

            // Custom zoom slider — only visible in custom mode
            DSStyle.Slider {
                id: zoomSlider
                visible: root.previewModel && root.previewModel.zoomMode === "custom"
                from: 0.5
                to: 3.0
                stepSize: 0.25
                value: root.previewModel ? root.previewModel.zoomFactor : 1.0
                implicitWidth: 80
                onMoved: {
                    if (root.previewModel) root.previewModel.zoomFactor = value
                }
            }
        }

        // ── Page image area ───────────────────────────────────────────────
        Item {
            id: previewImageArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Rendered page image
            Image {
                id: pageImage
                anchors.fill: parent
                fillMode: {
                    if (!root.previewModel) return Image.PreserveAspectFit
                    switch (root.previewModel.zoomMode) {
                    case "fitWidth": return Image.PreserveAspectCrop
                    case "custom":   return Image.Pad
                    default:         return Image.PreserveAspectFit
                    }
                }
                smooth: true
                source: (root.previewModel && root.previewModel.currentPageDataUrl)
                        ? root.previewModel.currentPageDataUrl
                        : ""
                visible: source !== "" && status !== Image.Error
            }

            // Loading / rendering spinner
            Item {
                anchors.centerIn: parent
                visible: root.previewModel
                         && (root.previewModel.loading || root.previewModel.rendering)
                         && pageImage.source === ""

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    BusyIndicator {
                        anchors.horizontalCenter: parent.horizontalCenter
                        running: parent.parent.visible
                    }

                    DSStyle.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Rendering…")
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                    }
                }
            }

            // Error state
            Column {
                anchors.centerIn: parent
                visible: root.previewModel
                         && root.previewModel.errorString !== ""
                         && !root.previewModel.loading
                         && !root.previewModel.rendering
                spacing: 6

                DSStyle.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "⚠"
                    font.pixelSize: 28
                    color: Theme.color("error")
                }
                DSStyle.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.previewModel ? root.previewModel.errorString : ""
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                    wrapMode: Text.WordWrap
                    width: Math.min(previewImageArea.width - 32, 260)
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // No-document placeholder (shown when source is empty and not loading)
            Column {
                anchors.centerIn: parent
                visible: (!root.previewModel || root.previewModel.documentUri === "")
                         && !root.previewModel?.loading
                spacing: 6

                DSStyle.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "📄"
                    font.pixelSize: 36
                    color: Theme.color("textTertiary")
                }
                DSStyle.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("No document")
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                }
            }
        }
    }
}
