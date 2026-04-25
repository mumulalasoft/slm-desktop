import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import SlmStyle

Window {
    id: root

    // ── public API ────────────────────────────────────────────────────────────
    property string ssid: ""
    property string securityType: "WPA2"    // "WPA2", "WPA3", "WEP", ""
    property int    signalStrength: 75      // 0–100
    property bool   isSecure: true
    property bool   busy: false
    property string errorMessage: ""
    property bool   rememberNetwork: true
    property bool   passwordRevealed: false
    property bool   isClosing: false

    signal accepted(string password, bool remember)
    signal rejected()

    // ── geometry ──────────────────────────────────────────────────────────────
    width: 340
    minimumWidth: 320
    maximumWidth: 400
    minimumHeight: 300
    maximumHeight: 500
    height: Math.max(minimumHeight,
                     Math.min(maximumHeight, contentColumn.implicitHeight + Theme.spacingXxl * 2))

    // ── window chrome ─────────────────────────────────────────────────────────
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    title: qsTr("Join Wi-Fi Network")
    color: "transparent"
    visible: false

    // ── theme shortcuts ───────────────────────────────────────────────────────
    property color surface:         Theme.color("surface")
    property color surfaceBorder:   Theme.color("panelBorder")
    property color textPrimary:     Theme.color("textPrimary")
    property color textMuted:       Theme.color("textSecondary")
    property color textDisabled:    Theme.color("textDisabled")
    property color accent:          Theme.color("accent")
    property color accentText:      Theme.color("accentText")
    property color danger:          Theme.color("error")
    property color fieldBg:         Theme.color("controlBg")
    property color fieldDisabledBg: Theme.color("controlDisabledBg")

    // ── shake timing ──────────────────────────────────────────────────────────
    readonly property int shakeDurA: Math.max(1, Math.round(Theme.durationMicro * 0.42))
    readonly property int shakeDurB: Math.max(1, Math.round(Theme.durationMicro * 0.58))
    readonly property int shakeDurC: Math.max(1, Math.round(Theme.durationMicro * 0.51))
    readonly property int shakeDurD: Math.max(1, Math.round(Theme.durationMicro * 0.44))
    readonly property int shakeDurE: Math.max(1, Math.round(Theme.durationMicro * 0.36))

    // ── helpers ───────────────────────────────────────────────────────────────
    function wifiIconCandidates() {
        var s = Number(signalStrength)
        var level = "none"
        if (s >= 75)      level = "excellent"
        else if (s >= 50) level = "good"
        else if (s >= 25) level = "ok"
        else if (s > 0)   level = "weak"

        if (isSecure) {
            return [
                "network-wireless-signal-" + level + "-secure-symbolic",
                "network-wireless-signal-" + level + "-symbolic",
                "network-wireless-signal-good-symbolic",
                "network-wireless-symbolic"
            ]
        }
        return [
            "network-wireless-signal-" + level + "-symbolic",
            "network-wireless-signal-good-symbolic",
            "network-wireless-symbolic"
        ]
    }

    function submitIfPossible() {
        if (root.busy) return
        if (passwordField.text.length === 0) { passwordField.forceActiveFocus(); return }
        root.accepted(passwordField.text, root.rememberNetwork)
    }

    function applyThemePreferences() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return
        Theme.applyModeString(DesktopSettings.themeMode)
        Theme.userAccentColor = DesktopSettings.accentColor
        Theme.userFontScale   = DesktopSettings.fontScale
    }

    // ── show / dismiss API ────────────────────────────────────────────────────
    function showDialog() {
        passwordRevealed = false
        isClosing = false
        showAnim.stop(); hideAnim.stop()
        visible = true
        showAnim.start()
    }

    function dismissDialog() {
        if (isClosing || !visible) return
        hideAnim.start()
    }

    // ── lifecycle ─────────────────────────────────────────────────────────────
    Component.onCompleted: applyThemePreferences()

    onVisibleChanged: {
        if (!visible) {
            passwordField.clear()
            passwordRevealed = false
            return
        }
        passwordField.forceActiveFocus()
    }

    function onClosing(close) {
        if (root.busy)      { close.accepted = false; return }
        if (!root.isClosing) {
            close.accepted = false
            root.rejected()
            root.dismissDialog()
        } else {
            close.accepted = true
        }
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
                NumberAnimation { target: panel; property: "opacity"; from: 0.0;        to: 1.0;  duration: Theme.durationSm;   easing.type: Theme.easingDefault    }
                NumberAnimation { target: panel; property: "scale";   from: 0.97;       to: 1.0;  duration: Theme.durationMd;   easing.type: Theme.easingDefault    }
            }
        }
        SequentialAnimation {
            id: hideAnim
            PropertyAction  { target: root;  property: "isClosing"; value: true }
            ParallelAnimation {
                NumberAnimation { target: panel; property: "opacity"; from: panel.opacity; to: 0.0;  duration: Theme.durationFast; easing.type: Theme.easingAccelerate }
                NumberAnimation { target: panel; property: "scale";   from: panel.scale;   to: 0.98; duration: Theme.durationFast; easing.type: Theme.easingAccelerate }
            }
            PropertyAction  { target: root;  property: "visible";   value: false }
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

            // ── wifi icon badge ────────────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(Theme.fontPxJumbo * 2.0)
                Layout.bottomMargin: Theme.spacingMd

                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.round(Theme.fontPxJumbo * 1.78)
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05)
                }

                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.round(Theme.fontPxJumbo * 1.44)
                    height: width
                    radius: width / 2
                    color:        Theme.darkMode ? Qt.rgba(1, 1, 1, 0.08)  : Qt.rgba(0, 0, 0, 0.045)
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.13)  : Qt.rgba(0, 0, 0, 0.09)

                    Image {
                        id: wifiIcon
                        anchors.centerIn: parent
                        width:  Theme.fontSize("xl")
                        height: width
                        property var candidates: root.wifiIconCandidates()
                        property int candidateIndex: 0
                        source: "image://themeicon/" + (candidates[candidateIndex] || "") + "?v=" +
                                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                 ? ThemeIconController.revision : 0)
                        fillMode: Image.PreserveAspectFit
                        visible: status !== Image.Error && status !== Image.Null
                        onStatusChanged: {
                            if (status === Image.Error && candidateIndex + 1 < candidates.length)
                                candidateIndex += 1
                        }
                        onCandidatesChanged: candidateIndex = 0
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !wifiIcon.visible
                        text: root.isSecure ? "🔒" : "📶"
                        font.pixelSize: Theme.fontSize("xl")
                    }
                }
            }

            // ── title ──────────────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingXs
                text: root.ssid.length > 0
                      ? qsTr("Join “%1”").arg(root.ssid)
                      : qsTr("Join Wi-Fi Network")
                color: root.textPrimary
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("bold")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            // ── subtitle ───────────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingMd
                text: qsTr("Enter the Wi-Fi password to connect.")
                color: root.textMuted
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            // ── security type chip ─────────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingLg
                implicitHeight: secChip.implicitHeight
                visible: root.securityType.length > 0

                Rectangle {
                    id: secChip
                    anchors.horizontalCenter: parent.horizontalCenter
                    implicitWidth:  secChipLabel.implicitWidth + Theme.spacingMd * 2
                    implicitHeight: Theme.controlHeightCompact
                    radius: Theme.radiusPill
                    color:        Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07)  : Qt.rgba(0, 0, 0, 0.045)
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.11)  : Qt.rgba(0, 0, 0, 0.09)

                    Label {
                        id: secChipLabel
                        anchors.centerIn: parent
                        text: root.securityType
                        color: root.textDisabled
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Theme.fontWeight("medium")
                    }
                }
            }

            // ── password field ─────────────────────────────────────────────────
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
                    KeyNavigation.tab: joinButton
                    KeyNavigation.backtab: cancelButton
                    echoMode: root.passwordRevealed ? TextInput.Normal : TextInput.Password
                    enabled: !root.busy
                    placeholderText: qsTr("Password")
                    selectByMouse: true
                    onAccepted: joinButton.clicked()
                    color: root.textPrimary
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding:  Theme.spacingLg + Theme.fontPxSm + Theme.spacingSm
                    rightPadding: Theme.spacingXl  + Theme.spacingLg
                    background: Rectangle {
                        radius: Theme.radiusPill
                        color: passwordField.enabled ? root.fieldBg : root.fieldDisabledBg
                        border.width: passwordField.visualFocus ? Theme.borderWidthThick : Theme.borderWidthThin
                        border.color: passwordField.visualFocus ? Theme.color("focusRing")
                                      : (root.errorMessage.length > 0 ? root.danger : root.surfaceBorder)
                        Behavior on border.color { ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                    }
                }

                Label {
                    anchors { left: parent.left; leftMargin: Theme.spacingLg; verticalCenter: parent.verticalCenter }
                    text: "🔑"
                    font.pixelSize: Theme.fontSize("small")
                    opacity: passwordField.enabled ? 0.46 : 0.22
                    Behavior on opacity { NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                }

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

            // ── error label ────────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingMd
                visible: root.errorMessage.length > 0
                text: root.errorMessage
                color: root.danger
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("xs")
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            // ── remember this network ──────────────────────────────────────────
            CheckBox {
                id: rememberCheck
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: Theme.spacingLg
                text: qsTr("Remember this network")
                checked: root.rememberNetwork
                enabled: !root.busy
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                onToggled: root.rememberNetwork = checked
                contentItem: Text {
                    leftPadding: rememberCheck.indicator ? rememberCheck.indicator.width + rememberCheck.spacing : 0
                    text: rememberCheck.text
                    color: root.textMuted
                    font: rememberCheck.font
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // ── buttons ────────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingXxl
                spacing: Theme.spacingMd

                Button {
                    id: cancelButton
                    text: qsTr("Cancel")
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    KeyNavigation.tab: passwordField
                    KeyNavigation.backtab: joinButton
                    enabled: !root.busy
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
                        root.rejected()
                        root.dismissDialog()
                    }
                }

                Button {
                    id: joinButton
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    KeyNavigation.tab: cancelButton
                    KeyNavigation.backtab: passwordField
                    enabled: !root.busy && passwordField.text.length > 0
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("medium")
                    Layout.fillWidth: true
                    implicitHeight: Theme.controlHeightLarge
                    background: Rectangle {
                        radius: Theme.radiusMdPlus
                        color: !joinButton.enabled ? Theme.color("controlDisabledBg")
                             : joinButton.down     ? Qt.darker( root.accent, Theme.darkMode ? 1.28 : 1.18)
                             : joinButton.hovered  ? Qt.lighter(root.accent, Theme.darkMode ? 1.14 : 1.08)
                             : root.accent
                        border.width: joinButton.visualFocus ? Theme.borderWidthThick : Theme.borderWidthNone
                        border.color: joinButton.visualFocus ? Theme.color("focusRing") : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                    }
                    contentItem: Item {
                        Item {
                            anchors.centerIn: parent
                            visible: root.busy
                            width: Theme.spacingXl; height: Theme.spacingXl
                            RotationAnimator on rotation {
                                running: root.busy
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
                        Text {
                            anchors.centerIn: parent
                            visible: !root.busy
                            text: qsTr("Join")
                            color: !joinButton.enabled ? Theme.color("textDisabled") : root.accentText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment:   Text.AlignVCenter
                            font: joinButton.font
                            fontSizeMode: Text.Fit
                            minimumPixelSize: 12
                        }
                    }
                    onClicked: root.submitIfPossible()
                }
            }
        } // ColumnLayout
    } // Rectangle (panel)

    // ── error shake reaction ──────────────────────────────────────────────────
    onErrorMessageChanged: {
        if (root.errorMessage.length === 0) return
        shakeAnim.restart()
        passwordField.forceActiveFocus()
        passwordField.selectAll()
    }

    // ── theme reactivity ──────────────────────────────────────────────────────
    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onThemeModeChanged()   { Theme.applyModeString(DesktopSettings.themeMode) }
        function onAccentColorChanged() { Theme.userAccentColor = DesktopSettings.accentColor }
        function onFontScaleChanged()   { Theme.userFontScale   = DesktopSettings.fontScale   }
    }

    // ── keyboard ──────────────────────────────────────────────────────────────
    Shortcut { sequence: "Return"; enabled: root.visible && !root.isClosing; onActivated: root.submitIfPossible() }
    Shortcut { sequence: "Enter";  enabled: root.visible && !root.isClosing; onActivated: root.submitIfPossible() }
    Shortcut {
        sequence: "Escape"
        enabled: root.visible && !root.isClosing && !root.busy
        onActivated: { root.rejected(); root.dismissDialog() }
    }
}
