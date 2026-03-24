import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

Item {
    id: root

    property var soundManager: SoundManager
    property var popupHost: null
    property var mediaSessionManager: MediaSessionManager
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingXs")
    readonly property int rowGap: Theme.metric("spacingMd")
    readonly property int sectionGap: Theme.metric("spacingSm")
    readonly property bool popupOpen: popupHint || menu.opened

    Timer {
        id: popupHintTimer
        interval: 320
        repeat: false
        onTriggered: root.popupHint = false
    }

    implicitWidth: indicatorButton.implicitWidth
    implicitHeight: indicatorButton.implicitHeight

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) {
            return
        }
        root.popupHint = true
        popupHintTimer.restart()
        Qt.callLater(function() { menu.open() })
    }

    function iconSourceByName(name) {
        var iconName = (name || "").toString().trim()
        if (iconName.length === 0) {
            iconName = "audio-volume-muted-symbolic"
        }
        return "image://themeicon/" + iconName + "?v=" +
                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                 ? ThemeIconController.revision : 0)
    }

    function indexOfByKey(list, key, value) {
        if (!list) {
            return -1
        }
        for (var i = 0; i < list.length; ++i) {
            if (list[i] && list[i][key] === value) {
                return i
            }
        }
        return -1
    }

    ToolButton {
        id: indicatorButton
        anchors.fill: parent
        padding: 0
        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) {
                return
            }
            if (menu.opened || menu.visible) {
                root.lastMenuCloseMs = Date.now()
                menu.close()
                return
            }
            root.openMenuSafely()
        }

        contentItem: Item {
            implicitWidth: root.iconSize
            implicitHeight: root.iconSize

            IconImage {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: root.iconSourceByName(root.soundManager ? root.soundManager.iconName : "audio-volume-muted-symbolic")
                color: Theme.color("textOnGlass")
                opacity: (root.soundManager && root.soundManager.available) ? 1.0 : 0.6
            }
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: indicatorButton.hovered ? Theme.color("accentSoft") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: indicatorButton.hovered ? Theme.color("panelBorder") : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.durationSm; easing.type: Easing.OutCubic } }
            Behavior on border.color { ColorAnimation { duration: Theme.durationSm; easing.type: Easing.OutCubic } }
        }
    }

    IndicatorMenu {
        id: menu
        anchorItem: indicatorButton
        popupGap: root.popupGap
        popupWidth: Theme.metric("popupWidthL")
        padding: 8
        onAboutToShow: {
            root.popupHint = false
            if (root.soundManager) {
                root.soundManager.refresh()
            }
            if (root.mediaSessionManager) {
                root.mediaSessionManager.refresh()
            }
        }
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            enabled: root.soundManager && root.soundManager.available
            contentItem: RowLayout {
                spacing: root.rowGap

                ToolButton {
                    icon.source: root.iconSourceByName(root.soundManager ? root.soundManager.iconName : "audio-volume-muted-symbolic")
                    onClicked: {
                        if (root.soundManager) {
                            root.soundManager.setMuted(!root.soundManager.muted)
                        }
                    }
                }

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 150
                    value: root.soundManager ? root.soundManager.volume : 0
                    live: true
                    onMoved: {
                        if (root.soundManager) {
                            root.soundManager.setVolume(Math.round(value))
                        }
                    }
                }

                Label {
                    text: root.soundManager ? (root.soundManager.volume + "%") : "--%"
                    color: Theme.color("textPrimary")
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            enabled: root.soundManager && root.soundManager.available
            contentItem: RowLayout {
                spacing: root.sectionGap
                Label {
                    text: "Output"
                    color: Theme.color("textSecondary")
                    Layout.preferredWidth: 56
                }
                ComboBox {
                    id: sinkCombo
                    Layout.fillWidth: true
                    model: root.soundManager ? root.soundManager.sinks : []
                    textRole: "description"
                    onActivated: {
                        if (!root.soundManager || currentIndex < 0 || currentIndex >= model.length) {
                            return
                        }
                        var selected = model[currentIndex]
                        if (selected && selected.name) {
                            root.soundManager.setDefaultSink(selected.name)
                        }
                    }
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            visible: root.soundManager && root.soundManager.streams &&
                     root.soundManager.streams.length > 0
            enabled: visible
            contentItem: ColumnLayout {
                spacing: root.sectionGap
                Label {
                    text: "Applications"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("body")
                }
                Repeater {
                    model: root.soundManager ? root.soundManager.streams : []
                    delegate: RowLayout {
                        required property var modelData
                        spacing: root.sectionGap

                        ToolButton {
                            icon.source: root.iconSourceByName(modelData && modelData.muted
                                                               ? "audio-volume-muted-symbolic"
                                                               : "audio-volume-medium-symbolic")
                            onClicked: {
                                if (root.soundManager && modelData) {
                                    root.soundManager.setStreamMuted(modelData.id, !modelData.muted)
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 120
                            text: modelData && modelData.name ? modelData.name : "Application"
                            color: Theme.color("textPrimary")
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            id: activeDot
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: Theme.radiusKnob
                            color: Theme.color("success")
                            visible: modelData && modelData.active === true
                            opacity: visible ? 1.0 : 0.0
                            scale: visible ? 1.0 : 0.85

                            SequentialAnimation on opacity {
                                running: activeDot.visible
                                loops: Animation.Infinite
                                NumberAnimation { from: 0.45; to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 1.0; to: 0.45; duration: 700; easing.type: Easing.InOutQuad }
                            }

                            SequentialAnimation on scale {
                                running: activeDot.visible
                                loops: Animation.Infinite
                                NumberAnimation { from: 0.88; to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 1.0; to: 0.88; duration: 700; easing.type: Easing.InOutQuad }
                            }
                        }

                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 150
                            value: modelData && modelData.volume !== undefined ? modelData.volume : 0
                            live: true
                            onMoved: {
                                if (root.soundManager && modelData) {
                                    root.soundManager.setStreamVolume(modelData.id, Math.round(value))
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 42
                            horizontalAlignment: Text.AlignRight
                            text: modelData && modelData.volume !== undefined
                                  ? (Math.round(modelData.volume) + "%")
                                  : "--%"
                            color: Theme.color("textSecondary")
                        }
                    }
                }
            }
        }

        MenuSeparator {
            visible: root.soundManager && root.soundManager.streams &&
                     root.soundManager.streams.length > 0
        }

        MenuItem {
            visible: root.mediaSessionManager && root.mediaSessionManager.players &&
                     root.mediaSessionManager.players.length > 1
            enabled: visible
            contentItem: RowLayout {
                spacing: root.sectionGap
                Label {
                    text: "Player"
                    color: Theme.color("textSecondary")
                    Layout.preferredWidth: 56
                }
                ComboBox {
                    id: playerCombo
                    Layout.fillWidth: true
                    model: root.mediaSessionManager ? root.mediaSessionManager.players : []
                    textRole: "name"
                    onActivated: {
                        if (!root.mediaSessionManager || currentIndex < 0 || currentIndex >= model.length) {
                            return
                        }
                        var selected = model[currentIndex]
                        if (selected && selected.service) {
                            root.mediaSessionManager.setActivePlayer(selected.service)
                        }
                    }
                }
            }
        }

        MenuSeparator {
            visible: root.mediaSessionManager && root.mediaSessionManager.players &&
                     root.mediaSessionManager.players.length > 1
        }

        MenuItem {
            visible: root.mediaSessionManager && root.mediaSessionManager.hasActiveSession
            enabled: visible
            contentItem: ColumnLayout {
                spacing: Theme.metric("spacingXxs")
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.rowGap
                    Item {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radiusControl
                            color: Theme.color("panelBg")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("menuBorder")
                        }

                        Image {
                            id: artworkImage
                            anchors.fill: parent
                            anchors.margins: 2
                            fillMode: Image.PreserveAspectFit
                            source: root.mediaSessionManager ? root.mediaSessionManager.artworkUrl : ""
                            visible: source.toString().length > 0 && status === Image.Ready
                        }

                        Image {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            fillMode: Image.PreserveAspectFit
                            source: root.iconSourceByName("audio-x-generic-symbolic")
                            visible: !artworkImage.visible
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: root.mediaSessionManager ? root.mediaSessionManager.title : ""
                            color: Theme.color("textPrimary")
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Theme.fontWeight("bold")
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.mediaSessionManager ? root.mediaSessionManager.artist : ""
                            color: Theme.color("textSecondary")
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("small")
                            elide: Text.ElideRight
                            visible: text.length > 0
                        }
                    }
                }
                RowLayout {
                    spacing: 10
                    ToolButton {
                        text: "⏮"
                        enabled: root.mediaSessionManager && root.mediaSessionManager.canGoPrevious
                        onClicked: {
                            if (root.mediaSessionManager) {
                                root.mediaSessionManager.previous()
                            }
                        }
                    }
                    ToolButton {
                        text: (root.mediaSessionManager && root.mediaSessionManager.playbackStatus === "Playing") ? "⏸" : "▶"
                        enabled: root.mediaSessionManager && root.mediaSessionManager.canPlayPause
                        onClicked: {
                            if (root.mediaSessionManager) {
                                root.mediaSessionManager.playPause()
                            }
                        }
                    }
                    ToolButton {
                        text: "⏭"
                        enabled: root.mediaSessionManager && root.mediaSessionManager.canGoNext
                        onClicked: {
                            if (root.mediaSessionManager) {
                                root.mediaSessionManager.next()
                            }
                        }
                    }
                }
            }
        }

        MenuSeparator {
            visible: root.mediaSessionManager && root.mediaSessionManager.hasActiveSession
        }

        MenuItem {
            text: "Open Sound Settings"
            onTriggered: {
                if (root.soundManager) {
                    root.soundManager.openSoundSettings()
                }
            }
        }
    }

    Connections {
        target: root.soundManager
        function onChanged() {
            if (!root.soundManager) {
                return
            }
            var sinkList = root.soundManager.sinks || []
            var idx = root.indexOfByKey(sinkList, "name", root.soundManager.currentSink)
            if (idx >= 0) {
                sinkCombo.currentIndex = idx
            }
        }
    }

    Connections {
        target: root.mediaSessionManager
        function onChanged() {
            if (!root.mediaSessionManager) {
                return
            }
            var players = root.mediaSessionManager.players || []
            var idx = root.indexOfByKey(players, "service", root.mediaSessionManager.activeService)
            if (idx >= 0) {
                playerCombo.currentIndex = idx
            }
        }
    }
}
