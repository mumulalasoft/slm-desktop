import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style as DSStyle

// Renders the declarative "Printer Features" section driven by a PluginFeatureResolver.
//
// Required properties:
//   resolver       — PluginFeatureResolver* (C++ object)
//   settingsModel  — PrintSettingsModel* (C++ object)
//
// The section is hidden when resolver.descriptorValid is false.
// When a field value changes, both resolver.setFieldValue() and
// settingsModel.pluginFeatures are updated atomically.

Item {
    id: root

    required property var resolver
    required property var settingsModel

    // Collapsed by default so advanced users opt-in.
    property bool expanded: false

    implicitWidth:  parent ? parent.width : 300
    implicitHeight: visible
                    ? (headerRow.implicitHeight
                       + (expanded ? contentCol.implicitHeight + 8 : 0))
                    : 0

    visible: root.resolver && root.resolver.descriptorValid

    Behavior on implicitHeight {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    // ── Helpers ───────────────────────────────────────────────────────────

    function commitValue(fieldId, value) {
        if (!root.resolver || !root.settingsModel)
            return
        root.resolver.setFieldValue(fieldId, value)
        root.settingsModel.pluginFeatures = root.resolver.buildIppAttributes()
    }

    // ── Header ────────────────────────────────────────────────────────────

    RowLayout {
        id: headerRow
        width: parent.width
        spacing: 4

        DSStyle.Button {
            flat: true
            text: root.expanded
                  ? qsTr("▾ %1").arg(root.resolver ? root.resolver.descriptorTitle : "")
                  : qsTr("▸ %1").arg(root.resolver ? root.resolver.descriptorTitle : "")
            font.pixelSize: Theme.fontSize("small")
            implicitHeight: Theme.metric("controlHeightCompact")
            Accessible.role: Accessible.Button
            Accessible.name: root.resolver ? root.resolver.descriptorTitle : ""
            Accessible.description: root.expanded ? qsTr("Collapse printer features")
                                                   : qsTr("Expand printer features")
            onClicked: root.expanded = !root.expanded
        }
        Item { Layout.fillWidth: true }
    }

    // ── Field list ────────────────────────────────────────────────────────

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

            Repeater {
                model: root.resolver ? root.resolver.featureModel : []

                delegate: Item {
                    // Each delegate spans both columns via a sub-RowLayout placed
                    // manually using GridLayout.columnSpan.

                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    implicitHeight: fieldRow.implicitHeight

                    // Expose modelData fields as local aliases for readability.
                    readonly property string fieldId:   modelData.id           ?? ""
                    readonly property string fieldType: modelData.type         ?? ""
                    readonly property string fieldLabel:modelData.label        ?? ""
                    readonly property var    fieldVals: modelData.values       ?? []
                    readonly property var    curValue:  modelData.currentValue
                    readonly property int    rangeMin:  modelData.rangeMin     ?? 0
                    readonly property int    rangeMax:  modelData.rangeMax     ?? 100

                    RowLayout {
                        id: fieldRow
                        width: parent.width
                        spacing: Theme.metric("spacingSm")

                        DSStyle.Label {
                            text: fieldLabel
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            Layout.preferredWidth: 120
                            elide: Text.ElideRight
                            visible: fieldType !== "bool"
                        }

                        // bool → CheckBox
                        DSStyle.CheckBox {
                            visible: fieldType === "bool"
                            Layout.fillWidth: true
                            text: fieldLabel
                            font.pixelSize: Theme.fontSize("small")
                            checked: curValue === true || curValue === "true"
                            Accessible.role: Accessible.CheckBox
                            Accessible.name: fieldLabel
                            onToggled: root.commitValue(fieldId, checked)
                        }

                        // enum / segmented → ComboBox
                        DSStyle.ComboBox {
                            visible: fieldType === "enum" || fieldType === "segmented"
                            Layout.fillWidth: true
                            model: fieldVals
                            currentIndex: {
                                var idx = fieldVals.indexOf(String(curValue))
                                return idx >= 0 ? idx : 0
                            }
                            Accessible.role: Accessible.ComboBox
                            Accessible.name: fieldLabel
                            onActivated: root.commitValue(fieldId, String(fieldVals[currentIndex]))
                        }

                        // range → Slider + value label
                        RowLayout {
                            visible: fieldType === "range"
                            Layout.fillWidth: true
                            spacing: 4

                            DSStyle.Slider {
                                Layout.fillWidth: true
                                from: rangeMin
                                to: rangeMax
                                stepSize: 1
                                value: (typeof curValue === "number") ? curValue : rangeMin
                                Accessible.role: Accessible.Slider
                                Accessible.name: fieldLabel
                                onMoved: root.commitValue(fieldId, Math.round(value))
                            }
                            DSStyle.Label {
                                text: String(Math.round(
                                          (typeof curValue === "number") ? curValue : rangeMin))
                                font.pixelSize: Theme.fontSize("small")
                                color: Theme.color("textSecondary")
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 36
                            }
                        }
                    }
                }
            }
        }
    }
}
