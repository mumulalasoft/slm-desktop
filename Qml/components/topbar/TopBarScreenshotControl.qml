import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root

    property int iconButtonW: Theme.metric("controlHeightRegular")
    property int iconButtonH: Theme.metric("controlHeightCompact")
    property int iconGlyph: 18
    property int popupInset: Theme.metric("spacingLg")
    property int popupGap: Theme.metric("spacingSm")
    property int popupControlH: Theme.metric("controlHeightLarge")
    readonly property bool popupOpen: screenshotPopup.opened

    signal popupHintRequested()
    signal popupHintCleared()
    signal screenshotCaptureRequested(string mode, int delaySec, bool grabPointer, bool concealText)

    implicitWidth: iconButtonW
    implicitHeight: iconButtonH

    property double lastCloseMs: 0

    function recentlyClosed(debounceMs) {
        var d = debounceMs === undefined ? 220 : debounceMs
        return (Date.now() - Number(lastCloseMs || 0)) < d
    }

    function loadScreenshotPrefs() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return
        }
        var modeValue = String(UIPreferences.getPreference("screenshot.mode", "screen"))
        if (modeValue !== "screen" && modeValue !== "window" && modeValue !== "area") {
            modeValue = "screen"
        }
        screenshotPopup.mode = modeValue
        screenshotPopup.grabPointer = !!UIPreferences.getPreference("screenshot.grabPointer", false)
        screenshotPopup.concealText = !!UIPreferences.getPreference("screenshot.concealText", false)
        var delayValue = Number(UIPreferences.getPreference("screenshot.delaySec", 0))
        if (isNaN(delayValue)) {
            delayValue = 0
        }
        screenshotPopup.delaySec = Math.max(0, Math.min(30, Math.round(delayValue)))
    }

    function saveScreenshotPrefs() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.setPreference) {
            return
        }
        UIPreferences.setPreference("screenshot.mode", String(screenshotPopup.mode || "screen"))
        UIPreferences.setPreference("screenshot.grabPointer", !!screenshotPopup.grabPointer)
        UIPreferences.setPreference("screenshot.concealText", !!screenshotPopup.concealText)
        UIPreferences.setPreference("screenshot.delaySec", Math.max(0, Math.min(30, Number(screenshotPopup.delaySec || 0))))
    }

    function openPopup() {
        if (recentlyClosed(180)) {
            return
        }
        root.popupHintRequested()
        Qt.callLater(function() {
            screenshotPopup.open()
        })
    }

    Rectangle {
        id: screenshotButton
        anchors.fill: parent
        radius: Theme.radiusControl
        color: screenshotMouse.containsMouse ? Theme.color("accentSoft") : "transparent"

        Image {
            id: screenshotIcon
            anchors.centerIn: parent
            width: root.iconGlyph
            height: root.iconGlyph
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            source: "image://themeicon/applets-screenshooter-symbolic?v=" +
                    ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                     ? ThemeIconController.revision : 0)
        }

        Label {
            anchors.centerIn: parent
            visible: screenshotIcon.status !== Image.Ready
            text: "\u2398"
            color: Theme.color("textOnGlass")
            font.pixelSize: Theme.fontSize("bodyLarge")
        }

        MouseArea {
            id: screenshotMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: function(mouse) {
                if (screenshotPopup.opened || screenshotPopup.visible) {
                    root.lastCloseMs = Date.now()
                    screenshotPopup.close()
                    mouse.accepted = true
                    return
                }
                if (root.recentlyClosed(220)) {
                    mouse.accepted = true
                    return
                }
                root.openPopup()
                mouse.accepted = true
            }
        }

        Popup {
            id: screenshotPopup
            modal: false
            focus: false
            dim: false
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            padding: 0
            width: Theme.metric("popupWidthM")
            x: Math.round(screenshotButton.mapToGlobal(0, 0).x + screenshotButton.width - width)
            y: Math.round(screenshotButton.mapToGlobal(0, 0).y + screenshotButton.height + Theme.metric("spacingSm"))
            onAboutToShow: root.popupHintCleared()
            onOpened: root.loadScreenshotPrefs()
            onAboutToHide: {
                root.popupHintCleared()
                root.lastCloseMs = Date.now()
            }

            property string mode: "screen"
            property bool grabPointer: false
            property bool concealText: false
            property int delaySec: 0
            onModeChanged: root.saveScreenshotPrefs()
            onGrabPointerChanged: root.saveScreenshotPrefs()
            onConcealTextChanged: root.saveScreenshotPrefs()
            onDelaySecChanged: root.saveScreenshotPrefs()

            background: DSStyle.PopupSurface {
                popupOpacity: Theme.popupSurfaceOpacityStrong
            }

            contentItem: Item {
                implicitWidth: contentColumn.implicitWidth + (root.popupInset * 2)
                implicitHeight: contentColumn.implicitHeight + (root.popupInset * 2)

                Column {
                    id: contentColumn
                    anchors.fill: parent
                    anchors.margins: root.popupInset
                    spacing: Theme.metric("spacingSm")

                    DSStyle.Label {
                        text: "Take Screenshot"
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("body")
                        font.weight: Theme.fontWeight("bold")
                    }

                    Rectangle {
                        width: contentColumn.width
                        height: root.popupControlH
                        radius: Theme.radiusControlLarge
                        color: Theme.color("fileManagerControlBg")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("fileManagerControlBorder")

                        Row {
                            anchors.fill: parent
                            anchors.margins: Theme.metric("spacingXxs")
                            spacing: Theme.metric("spacingXxs")

                            Repeater {
                                model: [
                                    { "id": "screen", "label": "Screen" },
                                    { "id": "window", "label": "Window" },
                                    { "id": "area", "label": "Area" }
                                ]

                                delegate: Rectangle {
                                    required property var modelData
                                    width: (parent.width - (Theme.metric("spacingXxs") * 2)) / 3
                                    height: parent.height
                                    radius: Theme.radiusControl
                                    color: screenshotPopup.mode === modelData.id
                                           ? Theme.color("fileManagerControlActive")
                                           : "transparent"

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: screenshotPopup.mode === modelData.id
                                               ? Theme.color("textPrimary")
                                               : Theme.color("textSecondary")
                                        font.pixelSize: Theme.fontSize("small")
                                        font.weight: (screenshotPopup.mode === modelData.id)
                                                     ? Theme.fontWeight("bold") : Theme.fontWeight("normal")
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: screenshotPopup.mode = parent.modelData.id
                                    }
                                }
                            }
                        }
                    }

                    DSStyle.IndicatorSectionRow {
                        width: contentColumn.width
                        text: "Grab pointer"
                        rowSpacing: root.popupGap
                        DSStyle.Switch {
                            checked: screenshotPopup.grabPointer
                            onToggled: screenshotPopup.grabPointer = checked
                        }
                    }

                    DSStyle.IndicatorSectionRow {
                        width: contentColumn.width
                        text: "Conceal text"
                        rowSpacing: root.popupGap
                        DSStyle.Switch {
                            checked: screenshotPopup.concealText
                            onToggled: screenshotPopup.concealText = checked
                        }
                    }

                    Row {
                        width: contentColumn.width
                        spacing: Theme.metric("spacingXs")
                        DSStyle.Label {
                            text: "Delay"
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("small")
                            width: 68
                            verticalAlignment: Text.AlignVCenter
                        }
                        DSStyle.Button {
                            text: "-"
                            width: root.iconButtonW
                            height: root.iconButtonW
                            enabled: screenshotPopup.delaySec > 0
                            onClicked: screenshotPopup.delaySec = Math.max(0, screenshotPopup.delaySec - 1)
                        }
                        Rectangle {
                            width: 40
                            height: root.iconButtonW
                            radius: Theme.radiusControl
                            color: Theme.color("fileManagerControlBg")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("fileManagerControlBorder")
                            DSStyle.Label {
                                anchors.centerIn: parent
                                text: String(screenshotPopup.delaySec)
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("small")
                                font.weight: Theme.fontWeight("bold")
                            }
                        }
                        DSStyle.Button {
                            text: "+"
                            width: root.iconButtonW
                            height: root.iconButtonW
                            enabled: screenshotPopup.delaySec < 30
                            onClicked: screenshotPopup.delaySec = Math.min(30, screenshotPopup.delaySec + 1)
                        }
                    }

                    Rectangle {
                        width: contentColumn.width
                        height: 1
                        color: Theme.color("menuBorder")
                    }

                    Row {
                        width: contentColumn.width
                        spacing: root.popupGap
                        Item {
                            width: Math.max(0, contentColumn.width - 210)
                            height: 1
                        }
                        DSStyle.Button {
                            text: "Cancel"
                            width: 100
                            onClicked: screenshotPopup.close()
                        }
                        DSStyle.Button {
                            text: "Take Screenshot"
                            width: 102
                            onClicked: {
                                var modeValue = screenshotPopup.mode
                                var delayValue = screenshotPopup.delaySec
                                var grabValue = screenshotPopup.grabPointer
                                var concealValue = screenshotPopup.concealText
                                screenshotPopup.close()
                                Qt.callLater(function() {
                                    root.screenshotCaptureRequested(modeValue, delayValue, grabValue, concealValue)
                                })
                            }
                        }
                    }
                }
            }
        }
    }
}
