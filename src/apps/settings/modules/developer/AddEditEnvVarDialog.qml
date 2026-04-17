import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Dialog {
    id: dialog

    property bool isEdit: false       // false = Add, true = Edit
    property string editKey: ""       // key being edited (read-only in edit mode)

    // Read-back from dialog before accepting
    readonly property string resultKey:       keyField.text.trim()
    readonly property string resultValue:     valueField.text
    readonly property string resultComment:   commentField.text
    readonly property string resultMergeMode: mergeModeReplace.checked ? "replace"
                                            : mergeModeAppend.checked  ? "append"
                                            : "prepend"

    signal accepted()

    title: isEdit ? qsTr("Edit Variable") : qsTr("Add Variable")

    modal: true
    anchors.centerIn: Overlay.overlay

    width: Math.min(480, parent ? parent.width - 48 : 480)
    padding: 24

    // Populate fields when opening in edit mode
    function openForAdd() {
        isEdit = false
        editKey = ""
        keyField.text = ""
        valueField.text = ""
        commentField.text = ""
        mergeModeReplace.checked = true
        keyError.text = ""
        valueError.text = ""
        open()
        keyField.forceActiveFocus()
    }

    function openForEdit(key, value, comment, mergeMode) {
        isEdit = true
        editKey = key
        keyField.text = key
        valueField.text = value
        commentField.text = comment
        if (mergeMode === "prepend")      mergeModePrepend.checked = true
        else if (mergeMode === "append")  mergeModeAppend.checked  = true
        else                              mergeModeReplace.checked = true
        keyError.text = ""
        valueError.text = ""
        open()
        valueField.forceActiveFocus()
    }

    // Validation helpers
    function validateAndAccept() {
        keyError.text   = ""
        valueError.text = ""

        const kRes = EnvVarController.validateKey(keyField.text.trim())
        if (!kRes.valid) {
            keyError.text = kRes.message
            keyField.forceActiveFocus()
            return
        }

        const vRes = EnvVarController.validateValue(keyField.text.trim(), valueField.text)
        if (!vRes.valid) {
            valueError.text = vRes.message
            valueField.forceActiveFocus()
            return
        }

        dialog.accepted()
        close()
    }

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.color("surface")
        border.color: Theme.color("panelBorder")
        border.width: 1
    }

    header: Item {
        implicitHeight: titleRow.implicitHeight + 24
        RowLayout {
            id: titleRow
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 20
            anchors.topMargin: 20

            Text {
                text: dialog.title
                font.pixelSize: Theme.fontSize("body")
                font.weight: Font.DemiBold
                color: Theme.color("textPrimary")
                Layout.fillWidth: true
            }
            ToolButton {
                icon.name: "window-close"
                icon.width: 14; icon.height: 14
                implicitWidth: 28; implicitHeight: 28
                padding: 0
                onClicked: dialog.close()
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? Theme.color("hoverOverlay") : "transparent"
                }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 16

        // Key field
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            Text {
                text: qsTr("Key")
                font.pixelSize: Theme.fontSize("sm")
                font.weight: Font.Medium
                color: Theme.color("textPrimary")
            }

            TextField {
                id: keyField
                Layout.fillWidth: true
                placeholderText: "MY_VARIABLE"
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSize("sm")
                enabled: !dialog.isEdit
                opacity: dialog.isEdit ? 0.5 : 1.0
                text: dialog.editKey
                Keys.onReturnPressed: valueField.forceActiveFocus()
                onTextChanged: {
                    keyError.text = ""
                    // Live format check — uppercase
                    const res = EnvVarController.validateKey(text.trim())
                    if (text.length > 0 && !res.valid)
                        keyError.text = res.message
                    else if (res.severity !== "none" && res.severity !== "")
                        sensitiveHint.visible = true
                    else
                        sensitiveHint.visible = false
                }
            }

            // Sensitive key hint (shown even when valid)
            RowLayout {
                id: sensitiveHint
                visible: false
                spacing: 6
                Text {
                    text: "⚠"
                    font.pixelSize: Theme.fontSize("xs")
                    color: "#E8A000"
                }
                Text {
                    text: qsTr("This is a sensitive system variable. Changes take effect for apps opened after saving.")
                    font.pixelSize: Theme.fontSize("xs")
                    color: "#E8A000"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            Text {
                id: keyError
                visible: text !== ""
                color: Theme.color("destructive")
                font.pixelSize: Theme.fontSize("xs")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Uppercase letters, digits, and underscores only.")
                font.pixelSize: Theme.fontSize("xs")
                color: Theme.color("textDisabled")
                visible: !dialog.isEdit
            }
        }

        // Value field
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            Text {
                text: qsTr("Value")
                font.pixelSize: Theme.fontSize("sm")
                font.weight: Font.Medium
                color: Theme.color("textPrimary")
            }

            TextField {
                id: valueField
                Layout.fillWidth: true
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSize("sm")
                Keys.onReturnPressed: dialog.validateAndAccept()
                onTextChanged: valueError.text = ""
            }

            Text {
                id: valueError
                visible: text !== ""
                color: Theme.color("destructive")
                font.pixelSize: Theme.fontSize("xs")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        // Merge mode — only shown for PATH-like keys
        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true
            visible: {
                const k = keyField.text.trim()
                return k === "PATH" || k.endsWith("PATH") || k === "LD_LIBRARY_PATH"
                    || k === "PYTHONPATH" || k === "PERLLIB"
            }

            Text {
                text: qsTr("Merge mode")
                font.pixelSize: Theme.fontSize("sm")
                font.weight: Font.Medium
                color: Theme.color("textPrimary")
            }

            RowLayout {
                spacing: 16
                RadioButton {
                    id: mergeModeReplace
                    text: qsTr("Replace")
                    checked: true
                    font.pixelSize: Theme.fontSize("sm")
                }
                RadioButton {
                    id: mergeModePrepend
                    text: qsTr("Prepend")
                    font.pixelSize: Theme.fontSize("sm")
                }
                RadioButton {
                    id: mergeModeAppend
                    text: qsTr("Append")
                    font.pixelSize: Theme.fontSize("sm")
                }
            }
            Text {
                text: mergeModeReplace.checked
                    ? qsTr("This value replaces any lower-priority entry.")
                    : mergeModePrepend.checked
                        ? qsTr("Prepended to lower-priority value with ':' separator.")
                        : qsTr("Appended to lower-priority value with ':' separator.")
                font.pixelSize: Theme.fontSize("xs")
                color: Theme.color("textDisabled")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        // Comment field
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            Text {
                text: qsTr("Comment") + " " + qsTr("(optional)")
                font.pixelSize: Theme.fontSize("sm")
                font.weight: Font.Medium
                color: Theme.color("textPrimary")
            }

            TextField {
                id: commentField
                Layout.fillWidth: true
                placeholderText: qsTr("What is this variable for?")
                font.pixelSize: Theme.fontSize("sm")
                Keys.onReturnPressed: dialog.validateAndAccept()
            }
        }
    }

    footer: Item {
        implicitHeight: footerRow.implicitHeight + 32
        RowLayout {
            id: footerRow
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            anchors.margins: 20
            anchors.bottomMargin: 16

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Cancel")
                onClicked: dialog.close()
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.hovered ? Theme.color("hoverOverlay") : "transparent"
                    border.color: Theme.color("panelBorder")
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textPrimary")
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                implicitWidth: 80
                implicitHeight: 34
            }

            Button {
                text: dialog.isEdit ? qsTr("Save") : qsTr("Add Variable")
                enabled: keyField.text.trim().length > 0
                onClicked: dialog.validateAndAccept()
                implicitWidth: dialog.isEdit ? 80 : 110
                implicitHeight: 34
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.enabled
                        ? (parent.hovered ? Theme.color("accentHover") : Theme.color("accent"))
                        : Theme.color("controlDisabledBg")
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSize("sm")
                    font.weight: Font.Medium
                    color: parent.enabled ? Theme.color("accentText") : Theme.color("textDisabled")
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
