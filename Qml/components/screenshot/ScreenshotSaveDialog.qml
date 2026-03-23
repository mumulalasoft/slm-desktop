import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop
import Style as DSStyle

Window {
    id: dialog

    property var parentWindow: null
    property string sourcePath: ""
    property string saveName: ""
    property string saveFolderText: ""
    property int typeIndex: 0
    property string errorCode: ""
    property string errorHint: ""
    property bool saveEnabled: true
    property string saveDisabledReason: ""
    property var outputTypes: []

    signal chooseFolderRequested()
    signal cancelRequested()
    signal saveRequested()
    signal saveNameEdited(string value)
    signal typeIndexChangedByUser(int indexValue)

    color: Theme.color("menuBg")
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    transientParent: parentWindow
    title: "Save Image as..."
    width: parentWindow ? Math.min(700, Math.max(600, parentWindow.width - 460)) : 640
    height: 420
    x: parentWindow ? parentWindow.x + Math.round((parentWindow.width - width) / 2) : 0
    y: parentWindow ? parentWindow.y + Math.round((parentWindow.height - height) / 2) : 0

    onVisibleChanged: {
        if (visible) {
            requestActivate()
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: dialog.visible
        onActivated: dialog.cancelRequested()
    }

    Shortcut {
        sequence: "Return"
        enabled: dialog.visible
                 && String(dialog.saveName || "").trim().length > 0
                 && !!dialog.saveEnabled
        onActivated: dialog.saveRequested()
    }

    Shortcut {
        sequence: "Enter"
        enabled: dialog.visible
                 && String(dialog.saveName || "").trim().length > 0
                 && !!dialog.saveEnabled
        onActivated: dialog.saveRequested()
    }

    Component {
        id: screenshotSaveBodyComponent

        Column {
            width: dialog.width - (Theme.metric("spacingMd") * 2)
            spacing: Theme.metric("spacingSm")

            Rectangle {
                width: parent.width
                height: 126
                radius: Theme.radiusControlLarge
                color: Theme.color("fileManagerWindowBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("fileManagerWindowBorder")
                clip: true

                Image {
                    anchors.fill: parent
                    anchors.margins: 6
                    fillMode: Image.PreserveAspectFit
                    source: String(dialog.sourcePath || "").length > 0
                            ? "file://" + dialog.sourcePath
                            : ""
                    asynchronous: true
                    cache: false
                }
            }

            GridLayout {
                width: parent.width
                columns: 3
                columnSpacing: Theme.metric("spacingSm")
                rowSpacing: Theme.metric("spacingSm")

                DSStyle.Label {
                    Layout.preferredWidth: 94
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    text: "Name"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("body")
                }
                DSStyle.TextField {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.metric("controlHeightRegular")
                    text: dialog.saveName
                    color: Theme.color("textPrimary")
                    selectionColor: Theme.color("accent")
                    selectedTextColor: Theme.color("accentText")
                    onTextEdited: dialog.saveNameEdited(text)
                }

                DSStyle.Label {
                    Layout.preferredWidth: 94
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    text: "File Type"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("body")
                }
                DSStyle.ComboBox {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.metric("controlHeightRegular")
                    model: dialog.outputTypes
                    textRole: "label"
                    currentIndex: dialog.typeIndex
                    onActivated: dialog.typeIndexChangedByUser(currentIndex)
                }

                DSStyle.Label {
                    Layout.preferredWidth: 94
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    text: "Folder"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("body")
                }
                DSStyle.TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.metric("controlHeightRegular")
                    readOnly: true
                    text: dialog.saveFolderText
                    color: Theme.color("textPrimary")
                    selectionColor: Theme.color("accent")
                    selectedTextColor: Theme.color("accentText")
                }
                DSStyle.Button {
                    Layout.preferredWidth: 98
                    Layout.preferredHeight: Theme.metric("controlHeightRegular")
                    text: "Choose..."
                    onClicked: dialog.chooseFolderRequested()
                }

                Item {
                    Layout.preferredWidth: 94
                    Layout.preferredHeight: 1
                }
                DSStyle.Label {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    visible: String(dialog.errorHint || "").trim().length > 0
                    text: String(dialog.errorHint || "")
                    color: Theme.color("warning")
                    font.pixelSize: Theme.fontSize("small")
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Component {
        id: screenshotSaveFooterComponent

        Row {
            width: dialog.width - (Theme.metric("spacingMd") * 2)
            height: 32
            spacing: Theme.metric("spacingSm")

            Item {
                width: Math.max(0, parent.width - 206)
                height: 1
            }

            DSStyle.Button {
                width: 96
                height: Theme.metric("controlHeightRegular")
                text: "Cancel"
                onClicked: dialog.cancelRequested()
            }
            DSStyle.Button {
                id: saveButton
                width: 96
                height: Theme.metric("controlHeightRegular")
                text: "Save"
                defaultAction: true
                enabled: String(dialog.saveName || "").trim().length > 0 && !!dialog.saveEnabled
                onClicked: dialog.saveRequested()

                ToolTip.visible: hovered && !enabled && String(dialog.saveDisabledReason || "").trim().length > 0
                ToolTip.text: String(dialog.saveDisabledReason || "")
                ToolTip.delay: 150
            }
        }
    }

    DSStyle.WindowDialogSurface {
        anchors.fill: parent
        title: "Save Image as..."
        bodyComponent: screenshotSaveBodyComponent
        footerComponent: screenshotSaveFooterComponent
    }
}
