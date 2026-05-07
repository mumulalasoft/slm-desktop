import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import SlmStyle

Window {
    id: root

    // ── geometry ──────────────────────────────────────────────────────────────
    width: 380
    minimumWidth: 340
    maximumWidth: 430
    minimumHeight: 300
    maximumHeight: 480
    height: Math.max(minimumHeight,
                     Math.min(maximumHeight, contentColumn.implicitHeight + Theme.spacingXxl * 2))

    // ── window chrome ─────────────────────────────────────────────────────────
    property bool controllerActive: authDialogController && authDialogController.active
    property bool isClosing: false
    property bool passwordRevealed: false

    readonly property bool hasError: !!authDialogController
                                     && authDialogController.errorMessage !== ""

    visible: controllerActive || isClosing
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal
    title: "Authentication Required"
    color: "transparent"

    // ── theme shortcuts ───────────────────────────────────────────────────────
    property color surface:               Theme.color("surface")
    property color surfaceBorder:         Theme.color("panelBorder")
    property color textPrimary:           Theme.color("textPrimary")
    property color textMuted:             Theme.color("textSecondary")
    property color textDisabled:          Theme.color("textDisabled")
    property color accent:                Theme.color("accent")
    property color accentText:            Theme.color("accentText")
    property color danger:                Theme.color("error")
    property color fieldBg:               Theme.color("controlBg")
    property color fieldDisabledBg:       Theme.color("controlDisabledBg")

    // ── shake timing ──────────────────────────────────────────────────────────
    readonly property int shakeDurA: Math.max(1, Math.round(Theme.durationMicro * 0.42))
    readonly property int shakeDurB: Math.max(1, Math.round(Theme.durationMicro * 0.58))
    readonly property int shakeDurC: Math.max(1, Math.round(Theme.durationMicro * 0.51))
    readonly property int shakeDurD: Math.max(1, Math.round(Theme.durationMicro * 0.44))
    readonly property int shakeDurE: Math.max(1, Math.round(Theme.durationMicro * 0.36))

    // ── helpers ───────────────────────────────────────────────────────────────
    function applyThemePreferences() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return
        Theme.applyModeString(DesktopSettings.themeMode)
        Theme.userAccentColor  = DesktopSettings.accentColor
        Theme.userFontScale    = DesktopSettings.fontScale
    }

    function simplifiedIdentity(rawIdentity) {
        const value = String(rawIdentity || "")
        if (value.length === 0) return ""
        const idx = value.lastIndexOf(":")
        return idx >= 0 && idx + 1 < value.length ? value.substring(idx + 1) : value
    }

    function commandHint(message) {
        const src = String(message || "")
        const a = src.indexOf("`")
        if (a < 0) return ""
        const b = src.indexOf("`", a + 1)
        return b > a ? src.substring(a + 1, b) : ""
    }

    function cleanedMessage(message) {
        const src = String(message || "").trim()
        if (src.length === 0) return ""
        return src.replace(/`[^`]*`/g, "")
                  .replace(/\s+([.!?,])/g, "$1")
                  .replace(/\s+/g, " ")
                  .trim()
    }

    function submitIfPossible() {
        if (!authDialogController || !authDialogController.active || authDialogController.busy) return
        if (passwordField.text.length === 0) { passwordField.forceActiveFocus(); return }
        authDialogController.authenticate(passwordField.text)
    }

    // ── lifecycle ─────────────────────────────────────────────────────────────
    Component.onCompleted: applyThemePreferences()

    onControllerActiveChanged: {
        if (controllerActive) {
            passwordRevealed = false
            isClosing = false
            showAnim.stop(); hideAnim.stop()
            showAnim.start()
            return
        }
        if (visible) { showAnim.stop(); hideAnim.start() }
    }

    function onClosing(close) {
        if (!authDialogController || !authDialogController.active) { close.accepted = true; return }
        if (authDialogController.busy)                              { close.accepted = false; return }
        authDialogController.cancel()
        close.accepted = false
    }

    onVisibleChanged: {
        if (!visible) { passwordField.clear(); passwordRevealed = false; return }
        passwordField.forceActiveFocus()
    }

    // ── panel ─────────────────────────────────────────────────────────────────
    Rectangle {
        id: panel
        anchors.fill: parent
        antialiasing: true
        color: root.surface
        radius: Theme.radiusWindowAlt
        border.color: root.surfaceBorder
        border.width: Theme.borderWidthThin
        opacity: 0
        scale: 0.97

        SequentialAnimation {
            id: showAnim
            PropertyAction  { target: root;  property: "isClosing"; value: false }
            ParallelAnimation {
                NumberAnimation { target: panel; property: "opacity"; from: 0.0;        to: 1.0;        duration: Theme.durationSm;   easing.type: Theme.easingDefault    }
                NumberAnimation { target: panel; property: "scale";   from: 0.97;       to: 1.0;        duration: Theme.durationMd;   easing.type: Theme.easingDefault    }
            }
        }
        SequentialAnimation {
            id: hideAnim
            PropertyAction  { target: root;  property: "isClosing"; value: true }
            ParallelAnimation {
                NumberAnimation { target: panel; property: "opacity"; from: panel.opacity; to: 0.0;     duration: Theme.durationFast; easing.type: Theme.easingAccelerate }
                NumberAnimation { target: panel; property: "scale";   from: panel.scale;   to: 0.98;    duration: Theme.durationFast; easing.type: Theme.easingAccelerate }
            }
            PropertyAction  { target: root;  property: "isClosing"; value: false }
        }

        // ── content column ────────────────────────────────────────────────────
        ColumnLayout {
            id: contentColumn
            anchors {
                left: parent.left;  right: parent.right;  top: parent.top
                leftMargin:  Theme.spacingXxl
                rightMargin: Theme.spacingXxl
                topMargin:   Theme.spacingXxl
            }
            spacing: 0

            // ── lock badge ────────────────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(Theme.fontPxJumbo * 2.0)
                Layout.bottomMargin: Theme.spacingMd

                // outer halo ring
                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.round(Theme.fontPxJumbo * 1.78)
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05)
                }

                // inner badge circle
                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.round(Theme.fontPxJumbo * 1.44)
                    height: width
                    radius: width / 2
                    color:        Theme.darkMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.045)
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(0, 0, 0, 0.09)

                    Label {
                        anchors.centerIn: parent
                        text: "🔒"
                        font.pixelSize: Theme.fontSize("xl")
                    }
                }
            }

            // ── title ─────────────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingXs
                text: "Authentication Required"
                color: root.textPrimary
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("bold")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            // ── polkit message ────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingXs
                property string _msg: authDialogController ? root.cleanedMessage(authDialogController.message) : ""
                text: _msg.length > 2 ? _msg : "An application requires your authorization."
                color: root.textMuted
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("regular")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            // ── identity ──────────────────────────────────────────────────────
            Item {
                id: identityRow
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingLg
                implicitHeight: Theme.fontPxXs + Theme.spacingXxs
                visible: identityLabel.text.length > 0

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.spacingXs

                    Label {
                        text: "as"
                        color: root.textDisabled
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("xs")
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        id: identityLabel
                        text: authDialogController ? root.simplifiedIdentity(authDialogController.identity) : ""
                        color: root.textMuted
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Theme.fontWeight("medium")
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── command hint chip ─────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingSm
                implicitHeight: cmdHintText.implicitHeight + Theme.spacingXs * 2
                radius: Theme.radiusMd
                color:        Theme.darkMode ? Qt.rgba(1, 1, 1, 0.05)  : Qt.rgba(0, 0, 0, 0.04)
                border.width: Theme.borderWidthThin
                border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.09)  : Qt.rgba(0, 0, 0, 0.07)
                visible: cmdHintText.text.length > 0

                Label {
                    id: cmdHintText
                    anchors {
                        left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                        leftMargin: Theme.spacingMd; rightMargin: Theme.spacingMd
                    }
                    text: authDialogController ? root.commandHint(authDialogController.message) : ""
                    color: root.textMuted
                    font.family: Theme.fontFamilyMonospace
                    font.pixelSize: Theme.fontSize("xs")
                    maximumLineCount: 1
                    elide: Text.ElideMiddle
                }
            }

            // ── info banner ───────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingSm
                implicitHeight: infoText.implicitHeight + Theme.spacingXs * 2
                radius: Theme.radiusMd
                color:        Theme.darkMode ? Qt.rgba(0.039, 0.518, 1.0, 0.12)
                                             : Qt.rgba(0.039, 0.518, 1.0, 0.07)
                border.width: Theme.borderWidthThin
                border.color: Qt.rgba(0.039, 0.518, 1.0, Theme.darkMode ? 0.28 : 0.22)
                visible: infoText.text.length > 0

                Label {
                    id: infoText
                    anchors {
                        left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                        leftMargin: Theme.spacingMd; rightMargin: Theme.spacingMd
                    }
                    text: authDialogController ? (authDialogController.infoMessage || "") : ""
                    color: Qt.rgba(0.039, 0.518, 1.0, Theme.darkMode ? 0.95 : 1.0)
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("xs")
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                }
            }

            // ── password field ────────────────────────────────────────────────
            Item {
                id: passwordShakeWrap
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingXs
                implicitHeight: Theme.controlHeightLarge
                x: 0

                SequentialAnimation {
                    id: shakeAnim
                    running: false
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to: -10; duration: root.shakeDurA; easing.type: Theme.easingLight }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to:   8; duration: root.shakeDurB; easing.type: Theme.easingLight }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to:  -6; duration: root.shakeDurC; easing.type: Theme.easingLight }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to:   4; duration: root.shakeDurD; easing.type: Theme.easingLight }
                    NumberAnimation { target: passwordShakeWrap; property: "x"; to:   0; duration: root.shakeDurE; easing.type: Theme.easingLight }
                }

                TextField {
                    id: passwordField
                    anchors.fill: parent
                    activeFocusOnTab: true
                    KeyNavigation.tab: okButton
                    KeyNavigation.backtab: cancelButton
                    echoMode: root.passwordRevealed ? TextInput.Normal : TextInput.Password
                    enabled: authDialogController && authDialogController.active && !authDialogController.busy
                    placeholderText: "Password"
                    selectByMouse: true
                    onAccepted: okButton.clicked()
                    color: root.textPrimary
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    // left room for key icon, right room for reveal toggle
                    leftPadding:  Theme.spacingLg + Theme.fontPxSm + Theme.spacingSm
                    rightPadding: Theme.spacingXl  + Theme.spacingLg
                    background: Rectangle {
                        radius: Theme.radiusPill
                        color: passwordField.enabled ? root.fieldBg : root.fieldDisabledBg
                        border.width: passwordField.visualFocus ? Theme.borderWidthThick : Theme.borderWidthThin
                        border.color: passwordField.visualFocus ? Theme.color("focusRing")
                                      : (root.hasError ? root.danger : root.surfaceBorder)
                        Behavior on border.color { ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                    }
                }

                // key icon
                Label {
                    anchors { left: parent.left; leftMargin: Theme.spacingLg; verticalCenter: parent.verticalCenter }
                    text: "🔑"
                    font.pixelSize: Theme.fontSize("small")
                    opacity: passwordField.enabled ? 0.46 : 0.22
                    Behavior on opacity { NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                }

                // reveal toggle
                Item {
                    id: revealToggle
                    anchors { right: parent.right; rightMargin: Theme.spacingSm; verticalCenter: parent.verticalCenter }
                    width: Theme.spacingXxl + Theme.spacingXs
                    height: width
                    visible: passwordField.text.length > 0 && passwordField.enabled
                    opacity: revealArea.containsMouse ? 1.0 : 0.46
                    Behavior on opacity { NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }

                    Label {
                        anchors.centerIn: parent
                        text: root.passwordRevealed ? "🙈" : "👁"
                        font.pixelSize: Theme.fontSize("small")
                    }
                    MouseArea {
                        id: revealArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.passwordRevealed = !root.passwordRevealed
                    }
                }
            }

            // ── error label ───────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingMd
                visible: text.length > 0
                text: authDialogController ? (authDialogController.errorMessage || "") : ""
                color: root.danger
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("xs")
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            // ── buttons ───────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingXxl
                spacing: Theme.spacingMd

                // Cancel — ghost style, secondary action
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
                    Layout.fillWidth: true
                    implicitHeight: Theme.controlHeightLarge
                    background: Rectangle {
                        radius: Theme.radiusMdPlus
                        color: cancelButton.down    ? Theme.color("controlBgPressed")
                             : cancelButton.hovered ? Theme.color("controlBgHover")
                             : "transparent"
                        border.width: cancelButton.visualFocus ? Theme.borderWidthThick : Theme.borderWidthThin
                        border.color: cancelButton.visualFocus ? Theme.color("focusRing") : root.surfaceBorder
                        Behavior on color { ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                    }
                    contentItem: Text {
                        text: cancelButton.text
                        color: root.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment:   Text.AlignVCenter
                        font: cancelButton.font
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 12
                    }
                    onClicked: {
                        if (!authDialogController) return
                        authDialogController.cancel()
                        passwordField.clear()
                    }
                }

                // Authenticate — filled accent, primary action; shows spinner when busy
                Button {
                    id: okButton
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
                    Layout.fillWidth: true
                    implicitHeight: Theme.controlHeightLarge
                    background: Rectangle {
                        radius: Theme.radiusMdPlus
                        color: !okButton.enabled ? Theme.color("controlDisabledBg")
                             : okButton.down     ? Qt.darker( root.accent, Theme.darkMode ? 1.28 : 1.18)
                             : okButton.hovered  ? Qt.lighter(root.accent, Theme.darkMode ? 1.14 : 1.08)
                             : root.accent
                        border.width: okButton.visualFocus ? Theme.borderWidthThick : Theme.borderWidthNone
                        border.color: okButton.visualFocus ? Theme.color("focusRing") : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                    }
                    contentItem: Item {
                        // spinner — visible while busy
                        Item {
                            anchors.centerIn: parent
                            visible: authDialogController && authDialogController.busy
                            width: Theme.spacingXl; height: Theme.spacingXl
                            RotationAnimator on rotation {
                                running: authDialogController && authDialogController.busy
                                from: 0; to: 360
                                loops: Animation.Infinite
                                duration: Theme.durationWorkspace * 2
                            }
                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.reset()
                                    const lw = 2.0
                                    const r  = (width - lw) / 2
                                    ctx.beginPath()
                                    ctx.arc(width / 2, height / 2, r, Math.PI * 0.2, Math.PI * 1.7, false)
                                    ctx.lineWidth = lw
                                    ctx.lineCap   = "round"
                                    ctx.strokeStyle = "rgba(255,255,255,0.90)"
                                    ctx.stroke()
                                }
                            }
                        }
                        // label — visible when idle
                        Text {
                            anchors.centerIn: parent
                            visible: !(authDialogController && authDialogController.busy)
                            text: "Authenticate"
                            color: !okButton.enabled ? Theme.color("textDisabled") : root.accentText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment:   Text.AlignVCenter
                            font: okButton.font
                            fontSizeMode: Text.Fit
                            minimumPixelSize: 12
                        }
                    }
                    onClicked: root.submitIfPossible()
                }
            }
        } // ColumnLayout
    } // Rectangle (panel)

    // ── reactions ─────────────────────────────────────────────────────────────
    Connections {
        target: authDialogController
        function onErrorMessageChanged() {
            if (!authDialogController || authDialogController.errorMessage === "") return
            shakeAnim.restart()
            passwordField.forceActiveFocus()
            passwordField.selectAll()
        }
    }

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onThemeModeChanged()   { Theme.applyModeString(DesktopSettings.themeMode) }
        function onAccentColorChanged() { Theme.userAccentColor = DesktopSettings.accentColor }
        function onFontScaleChanged()   { Theme.userFontScale   = DesktopSettings.fontScale   }
    }

    // ── keyboard ──────────────────────────────────────────────────────────────
    Shortcut { sequence: "Return"; enabled: root.visible; onActivated: root.submitIfPossible() }
    Shortcut { sequence: "Enter";  enabled: root.visible; onActivated: root.submitIfPossible() }
    Shortcut {
        sequence: "Escape"
        enabled: root.visible && !!authDialogController && authDialogController.active && !authDialogController.busy
        onActivated: authDialogController.cancel()
    }
    Shortcut { sequence: "Alt+O"; enabled: root.visible; onActivated: root.submitIfPossible() }
    Shortcut {
        sequence: "Alt+C"
        enabled: root.visible && !!authDialogController && authDialogController.active && !authDialogController.busy
        onActivated: authDialogController.cancel()
    }
}
