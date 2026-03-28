import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root
    width: 380
    minimumWidth: 340
    maximumWidth: 430
    minimumHeight: 320
    maximumHeight: 410
    height: Math.max(minimumHeight, Math.min(maximumHeight, contentColumn.implicitHeight + 18))
    property bool controllerActive: authDialogController && authDialogController.active
    property bool isClosing: false
    visible: controllerActive || isClosing
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    title: "Authentication Required"
    color: "transparent"
    property color surface: Theme.color("surface")
    property color surfaceBorder: Theme.color("panelBorder")
    property color textPrimary: Theme.color("textPrimary")
    property color textMuted: Theme.color("textSecondary")
    property color accent: Theme.color("accent")
    property color danger: Theme.color("error")
    property color fieldBg: Theme.color("controlBg")
    property color fieldDisabledBg: Theme.color("controlDisabledBg")
    property color btnSecondaryBg: Theme.color("surface")
    property color btnSecondaryHoverBg: Theme.color("controlBgHover")
    property color btnSecondaryPressedBg: Theme.color("controlBgPressed")
    property color okText: Theme.color("accentText")
    readonly property int formRowWidth: Math.max(250, root.width - 56)

    onControllerActiveChanged: {
        if (controllerActive) {
            isClosing = false
            showAnim.stop()
            hideAnim.stop()
            showAnim.start()
            return
        }
        if (visible) {
            showAnim.stop()
            hideAnim.start()
        }
    }

    onClosing: function(close) {
        if (!authDialogController || !authDialogController.active) {
            close.accepted = true
            return
        }
        if (authDialogController.busy) {
            close.accepted = false
            return
        }
        authDialogController.cancel()
        close.accepted = false
    }

    function submitIfPossible() {
        if (!authDialogController || !authDialogController.active || authDialogController.busy) {
            return
        }
        if (passwordField.text.length === 0) {
            passwordField.forceActiveFocus()
            return
        }
        authDialogController.authenticate(passwordField.text)
    }

    function simplifiedIdentity(rawIdentity) {
        const value = String(rawIdentity || "")
        if (value.length === 0) {
            return ""
        }
        const idx = value.lastIndexOf(":")
        return idx >= 0 && idx + 1 < value.length ? value.substring(idx + 1) : value
    }

    function commandHint(message) {
        const src = String(message || "")
        const a = src.indexOf("`")
        if (a < 0) {
            return ""
        }
        const b = src.indexOf("`", a + 1)
        if (b <= a) {
            return ""
        }
        return src.substring(a + 1, b)
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        antialiasing: true
        color: root.surface
        radius: Theme.radiusWindowAlt
        border.color: root.surfaceBorder
        border.width: Theme.borderWidthThin
        opacity: 0.0
        scale: 0.97

        SequentialAnimation {
            id: showAnim
            PropertyAction { target: root; property: "isClosing"; value: false }
            ParallelAnimation {
                NumberAnimation { target: panel; property: "opacity"; from: 0.0; to: 1.0; duration: 140; easing.type: Easing.OutCubic }
                NumberAnimation { target: panel; property: "scale"; from: 0.97; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
            }
        }

        SequentialAnimation {
            id: hideAnim
            PropertyAction { target: root; property: "isClosing"; value: true }
            ParallelAnimation {
                NumberAnimation { target: panel; property: "opacity"; from: panel.opacity; to: 0.0; duration: 120; easing.type: Easing.InCubic }
                NumberAnimation { target: panel; property: "scale"; from: panel.scale; to: 0.98; duration: 120; easing.type: Easing.InCubic }
            }
            PropertyAction { target: root; property: "isClosing"; value: false }
        }

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            anchors.leftMargin: Theme.metric("spacingLg")
            anchors.rightMargin: Theme.metric("spacingLg")
            anchors.bottomMargin: Theme.metric("spacingLg")
            anchors.topMargin: 6
            spacing: 3

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                Label {
                    anchors.centerIn: parent
                    text: "🔒"
                    font.pixelSize: 62
                }
            }

            Label {
                text: "Authenticate"
                color: root.textPrimary
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("bold")
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                Layout.topMargin: 2
            }

            Label {
                text: "Enter your password to continue."
                color: root.textPrimary
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("semibold")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                Layout.topMargin: 0
            }

            Label {
                text: authDialogController ? commandHint(authDialogController.message) : ""
                visible: text.length > 0
                color: root.textMuted
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("xs")
                maximumLineCount: 1
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                Layout.topMargin: -1
            }

            Item {
                id: passwordShakeWrap
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: root.formRowWidth
                implicitHeight: 34
                Layout.topMargin: 5
                x: 0

                SequentialAnimation {
                    id: shakeAnim
                    running: false
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to: -10; duration: 38; easing.type: Easing.OutQuad }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to: 8; duration: 52; easing.type: Easing.OutQuad }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to: -6; duration: 46; easing.type: Easing.OutQuad }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to: 4; duration: 40; easing.type: Easing.OutQuad }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to: 0; duration: 32; easing.type: Easing.OutQuad }
                }

                TextField {
                    id: passwordField
                    anchors.fill: parent
                    activeFocusOnTab: true
                    KeyNavigation.tab: okButton
                    KeyNavigation.backtab: cancelButton
                    echoMode: authDialogController && authDialogController.promptEchoOn ? TextInput.Normal : TextInput.Password
                    enabled: authDialogController && authDialogController.active && !authDialogController.busy
                    placeholderText: "Password"
                    selectByMouse: true
                    onAccepted: okButton.clicked()
                    color: root.textPrimary
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    padding: 8
                    background: Rectangle {
                        radius: Theme.radiusMdPlus
                        border.width: passwordField.visualFocus ? Theme.borderWidthThick : Theme.borderWidthThin
                        border.color: passwordField.visualFocus ? Theme.color("focusRing") : root.surfaceBorder
                        color: passwordField.enabled ? root.fieldBg : root.fieldDisabledBg
                    }
                }
            }

            Label {
                visible: authDialogController && authDialogController.errorMessage !== ""
                text: authDialogController ? authDialogController.errorMessage : ""
                color: root.danger
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("xs")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                Layout.topMargin: 2
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: root.formRowWidth
                spacing: Theme.metric("spacingMd")
                Layout.topMargin: 6

                RowLayout {
                    visible: authDialogController && authDialogController.busy
                    spacing: Theme.metric("spacingSm")
                    Layout.alignment: Qt.AlignVCenter

                    Item {
                        id: busySpinner
                        width: 18
                        height: 18

                        RotationAnimator on rotation {
                            running: authDialogController && authDialogController.busy
                            from: 0
                            to: 360
                            loops: Animation.Infinite
                            duration: 800
                        }

                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                const lineWidth = 2.2
                                const r = (width - lineWidth) / 2
                                const cx = width / 2
                                const cy = height / 2
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, Math.PI * 0.2, Math.PI * 1.7, false)
                                ctx.lineWidth = lineWidth
                                ctx.lineCap = "round"
                                ctx.strokeStyle = root.accent
                                ctx.stroke()
                            }
                        }
                    }

                    Label {
                        text: ""
                        color: root.textMuted
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("xs")
                    }
                }

                Button {
                    id: cancelButton
                    text: "Cancel"
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    KeyNavigation.tab: passwordField
                    KeyNavigation.backtab: okButton
                    enabled: authDialogController && authDialogController.active && !authDialogController.busy
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: (root.formRowWidth - Theme.metric("spacingMd")) / 2
                    implicitHeight: 32
                    background: Rectangle {
                        radius: Theme.radiusMdPlus
                        color: cancelButton.down ? root.btnSecondaryPressedBg
                              : (cancelButton.hovered ? root.btnSecondaryHoverBg : root.btnSecondaryBg)
                        border.width: cancelButton.visualFocus ? Theme.borderWidthThick : Theme.borderWidthThin
                        border.color: cancelButton.visualFocus ? Theme.color("focusRing") : root.surfaceBorder
                    }
                    contentItem: Text {
                        text: cancelButton.text
                        color: root.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font: cancelButton.font
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 14
                    }
                    onClicked: {
                        if (!authDialogController) {
                            return
                        }
                        authDialogController.cancel()
                        passwordField.clear()
                    }
                }

                Button {
                    id: okButton
                    text: "OK"
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    KeyNavigation.tab: cancelButton
                    KeyNavigation.backtab: passwordField
                    enabled: authDialogController
                             && authDialogController.active
                             && !authDialogController.busy
                             && passwordField.text.length > 0
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("medium")
                    Layout.preferredWidth: (root.formRowWidth - Theme.metric("spacingMd")) / 2
                    implicitHeight: 32
                    background: Rectangle {
                        radius: Theme.radiusMdPlus
                        color: !okButton.enabled ? Theme.color("controlDisabledBg")
                              : (okButton.down ? Qt.darker(root.accent, Theme.darkMode ? 1.28 : 1.18)
                                 : (okButton.hovered ? Qt.lighter(root.accent, Theme.darkMode ? 1.14 : 1.08) : root.accent))
                        border.width: okButton.visualFocus ? Theme.borderWidthThick : Theme.borderWidthNone
                        border.color: okButton.visualFocus ? Theme.color("focusRing") : "transparent"
                    }
                    contentItem: Text {
                        text: okButton.text
                        color: !okButton.enabled ? Theme.color("textDisabled") : root.okText
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font: okButton.font
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 14
                    }
                    onClicked: {
                        root.submitIfPossible()
                    }
                }
            }
        }
    }

    onVisibleChanged: {
        if (!visible) {
            passwordField.clear()
            return
        }
        passwordField.forceActiveFocus()
    }

    Connections {
        target: authDialogController
        function onErrorMessageChanged() {
            if (!authDialogController || authDialogController.errorMessage === "") {
                return
            }
            shakeAnim.restart()
            passwordField.forceActiveFocus()
            passwordField.selectAll()
        }
    }

    Shortcut {
        sequence: "Return"
        enabled: root.visible
        onActivated: root.submitIfPossible()
    }

    Shortcut {
        sequence: "Enter"
        enabled: root.visible
        onActivated: root.submitIfPossible()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible && authDialogController && authDialogController.active && !authDialogController.busy
        onActivated: authDialogController.cancel()
    }

    Shortcut {
        sequence: "Alt+O"
        enabled: root.visible
        onActivated: root.submitIfPossible()
    }

    Shortcut {
        sequence: "Alt+C"
        enabled: root.visible && authDialogController && authDialogController.active && !authDialogController.busy
        onActivated: authDialogController.cancel()
    }
}
