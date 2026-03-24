import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style as DSStyle

// Collapsible advanced settings panel for the print dialog.
//
// Required properties:
//   settingsModel   — PrintSettingsModel* (C++ object)
//   capability      — QVariantMap from PrinterManager.capabilities()
//   previewModel    — PrintPreviewModel* (for zoom mode control)

Item {
    id: root

    required property var settingsModel
    required property var capability
    required property var previewModel

    property bool expanded: false

    implicitWidth: parent ? parent.width : 300
    implicitHeight: headerRow.implicitHeight + (expanded ? contentCol.implicitHeight + 8 : 0)

    Behavior on implicitHeight {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    // ── Helpers ───────────────────────────────────────────────────────────

    readonly property var qualityModes: (capability && capability.qualityModes
                                         && capability.qualityModes.length > 0)
                                        ? capability.qualityModes
                                        : ["draft", "normal", "high"]

    readonly property var mediaSources: (capability && capability.mediaSources
                                         && capability.mediaSources.length > 0)
                                        ? capability.mediaSources : []

    readonly property var resolutionsDpi: (capability && capability.resolutionsDpi
                                           && capability.resolutionsDpi.length > 0)
                                          ? capability.resolutionsDpi : []

    // ── Expand / collapse toggle ───────────────────────────────────────────
    RowLayout {
        id: headerRow
        width: parent.width
        spacing: 4

        DSStyle.Button {
            text: root.expanded ? qsTr("▾ Advanced") : qsTr("▸ Advanced")
            flat: true
            font.pixelSize: Theme.fontSize("small")
            implicitHeight: Theme.metric("controlHeightCompact")
            onClicked: root.expanded = !root.expanded
        }
        Item { Layout.fillWidth: true }
    }

    // ── Content ───────────────────────────────────────────────────────────
    Column {
        id: contentCol
        anchors.top: headerRow.bottom
        anchors.topMargin: 4
        width: parent.width
        spacing: 0
        visible: root.expanded
        clip: true

        GridLayout {
            width: parent.width
            columns: 2
            columnSpacing: Theme.metric("spacingSm")
            rowSpacing: 6

            // Quality
            DSStyle.Label {
                text: qsTr("Quality")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                visible: root.qualityModes.length > 1
            }
            DSStyle.ComboBox {
                Layout.fillWidth: true
                visible: root.qualityModes.length > 1
                model: root.qualityModes
                currentIndex: {
                    if (!root.settingsModel) return 0
                    var v = String(root.settingsModel.quality)
                    var i = model.indexOf(v)
                    return i >= 0 ? i : 0
                }
                onActivated: {
                    if (root.settingsModel)
                        root.settingsModel.quality = String(model[currentIndex])
                }
            }

            // Media Source (tray)
            DSStyle.Label {
                text: qsTr("Paper Tray")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                visible: root.mediaSources.length > 0
            }
            DSStyle.ComboBox {
                Layout.fillWidth: true
                visible: root.mediaSources.length > 0
                textRole: "label"
                model: root.mediaSources
                currentIndex: {
                    if (!root.settingsModel) return 0
                    var current = String(root.settingsModel.mediaSource)
                    for (var i = 0; i < root.mediaSources.length; ++i) {
                        if (root.mediaSources[i].id === current) return i
                    }
                    return 0
                }
                onActivated: {
                    if (root.settingsModel && currentIndex >= 0
                            && currentIndex < root.mediaSources.length) {
                        root.settingsModel.mediaSource =
                            String(root.mediaSources[currentIndex].id)
                    }
                }
            }

            // Resolution
            DSStyle.Label {
                text: qsTr("Resolution")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                visible: root.resolutionsDpi.length > 0
            }
            DSStyle.ComboBox {
                Layout.fillWidth: true
                visible: root.resolutionsDpi.length > 0
                model: {
                    var items = [qsTr("Printer default")]
                    for (var i = 0; i < root.resolutionsDpi.length; ++i) {
                        items.push(root.resolutionsDpi[i] + " dpi")
                    }
                    return items
                }
                currentIndex: {
                    if (!root.settingsModel || root.settingsModel.resolutionDpi <= 0) return 0
                    var dpi = root.settingsModel.resolutionDpi
                    for (var i = 0; i < root.resolutionsDpi.length; ++i) {
                        if (root.resolutionsDpi[i] === dpi) return i + 1
                    }
                    return 0
                }
                onActivated: {
                    if (!root.settingsModel) return
                    if (currentIndex === 0) {
                        root.settingsModel.resolutionDpi = 0
                    } else {
                        var idx = currentIndex - 1
                        if (idx >= 0 && idx < root.resolutionsDpi.length)
                            root.settingsModel.resolutionDpi = root.resolutionsDpi[idx]
                    }
                }
            }

            // Scale
            DSStyle.Label {
                text: qsTr("Scale (%)")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }
            DSStyle.SpinBox {
                Layout.fillWidth: true
                from: 10
                to: 400
                value: root.settingsModel ? Math.round(root.settingsModel.scale) : 100
                onValueModified: {
                    if (root.settingsModel)
                        root.settingsModel.scale = value
                }
            }

            // Zoom mode (preview only)
            DSStyle.Label {
                text: qsTr("Preview Zoom")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }
            DSStyle.ComboBox {
                Layout.fillWidth: true
                model: [
                    { label: qsTr("Fit Page"),  value: "fitPage"  },
                    { label: qsTr("Fit Width"), value: "fitWidth" },
                    { label: qsTr("Custom"),    value: "custom"   }
                ]
                textRole: "label"
                currentIndex: {
                    if (!root.previewModel) return 0
                    switch (root.previewModel.zoomMode) {
                    case "fitWidth": return 1
                    case "custom":   return 2
                    default:         return 0
                    }
                }
                onActivated: {
                    if (root.previewModel)
                        root.previewModel.zoomMode = model[currentIndex].value
                }
            }
        }
    }
}
