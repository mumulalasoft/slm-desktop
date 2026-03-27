import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop

ApplicationWindow {
    id: window
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    width: 1000
    height: 720
    minimumWidth: 800
    minimumHeight: 560
    title: qsTr("System Settings")
    color: "transparent"
    background: Item {}

    readonly property bool windowMaximized: window.visibility === Window.Maximized
                                           || window.visibility === Window.FullScreen
    readonly property int shadowMargin: Theme.windowShadowMargin

    // Navigation state — true = home grid, false = module page
    property bool atHome: true

    readonly property var currentModule: {
        if (!SettingsApp || !SettingsApp.currentModuleId || !ModuleLoader) return null
        return ModuleLoader.moduleById(SettingsApp.currentModuleId) || null
    }

    function goHome() {
        window.atHome = true
    }

    function openModule(id) {
        if (!id || id.length === 0) return
        if (SettingsApp) SettingsApp.openModule(id)
        window.atHome = false
    }

    Component.onCompleted: {
        Theme.applyModeString(UIPreferences.themeMode)
        Theme.userAccentColor = UIPreferences.accentColor
        Theme.userFontScale   = UIPreferences.fontScale
    }

    Connections {
        target: UIPreferences
        function onThemeModeChanged()   { Theme.applyModeString(UIPreferences.themeMode) }
        function onAccentColorChanged() { Theme.userAccentColor = UIPreferences.accentColor }
        function onFontScaleChanged()   { Theme.userFontScale   = UIPreferences.fontScale }
    }

    // External module open (deep link / D-Bus)
    Connections {
        target: SettingsApp
        function onModuleOpened(moduleId) {
            if (moduleId && moduleId.length > 0) window.atHome = false
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: searchField.forceActiveFocus()
    }
    Shortcut {
        sequence: "Ctrl+K"
        onActivated: if (SettingsApp) SettingsApp.commandPaletteVisible = !SettingsApp.commandPaletteVisible
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (SettingsApp && SettingsApp.commandPaletteVisible) {
                SettingsApp.commandPaletteVisible = false
            } else if (!window.atHome) {
                window.goHome()
            }
        }
    }

    // ── Traffic-light button component ────────────────────────────────────
    component TitleButton: Item {
        required property string normalSrc
        required property string hoverSrc
        required property string activeSrc
        signal clicked()
        implicitWidth: 14; implicitHeight: 14
        property bool _hovered: false
        property bool _pressed: false
        Image {
            anchors.fill: parent
            source: parent._pressed ? parent.activeSrc
                                    : (parent._hovered ? parent.hoverSrc : parent.normalSrc)
            fillMode: Image.PreserveAspectFit
            smooth: true; antialiasing: true
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            onEntered:  parent._hovered = true
            onExited:   { parent._hovered = false; parent._pressed = false }
            onPressed:  parent._pressed = true
            onReleased: parent._pressed = false
            onClicked:  parent.clicked()
        }
    }

    // ── Shadow layers ─────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent; anchors.margins: 1
        radius: Theme.radiusWindow + 4
        color: Qt.rgba(0, 0, 0, Theme.windowShadowOuterOpacity)
        visible: !windowMaximized
    }
    Rectangle {
        anchors.fill: parent; anchors.margins: 3
        radius: Theme.radiusWindow + 2
        color: Qt.rgba(0, 0, 0, Theme.windowShadowInnerOpacity)
        visible: !windowMaximized
    }

    Rectangle {
        id: windowSurface
        anchors.fill: parent
        anchors.margins: windowMaximized ? 0 : shadowMargin
        color: Theme.color("windowBg")
        radius: Theme.radiusWindow
        clip: true
        antialiasing: true
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerWindowBorder")

        // Inner stroke
        Rectangle {
            anchors.fill: parent; radius: parent.radius
            color: "transparent"
            border.width: Theme.borderWidthThin
            border.color: Qt.rgba(0, 0, 0, Theme.windowInnerStrokeOpacity)
            antialiasing: true
            z: 1
        }

        // ── Title bar ─────────────────────────────────────────────────────
        // All children are direct siblings so anchoring between them is valid.
        Item {
            id: titleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 48
            z: 20

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active) window.startSystemMove()
                }
            }

            // ── Traffic lights (always visible) ──────────────────────────
            Row {
                id: trafficLights
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                TitleButton {
                    normalSrc: "qrc:/icons/titlebuttons/titlebutton-close.svg"
                    hoverSrc:  "qrc:/icons/titlebuttons/titlebutton-close-hover.svg"
                    activeSrc: "qrc:/icons/titlebuttons/titlebutton-close-active.svg"
                    onClicked: window.close()
                }
                TitleButton {
                    normalSrc: "qrc:/icons/titlebuttons/titlebutton-minimize.svg"
                    hoverSrc:  "qrc:/icons/titlebuttons/titlebutton-minimize-hover.svg"
                    activeSrc: "qrc:/icons/titlebuttons/titlebutton-minimize-active.svg"
                    onClicked: window.showMinimized()
                }
                TitleButton {
                    property bool isMax: window.visibility === Window.Maximized
                    normalSrc: "qrc:/icons/titlebuttons/titlebutton-maximize.svg"
                    hoverSrc: isMax ? "qrc:/icons/titlebuttons/titlebutton-unmaximize-hover.svg"
                                    : "qrc:/icons/titlebuttons/titlebutton-maximize-hover.svg"
                    activeSrc: isMax ? "qrc:/icons/titlebuttons/titlebutton-unmaximize-active.svg"
                                     : "qrc:/icons/titlebuttons/titlebutton-maximize-active.svg"
                    onClicked: isMax ? window.showNormal() : window.showMaximized()
                }
            }

            // ── Back button — sibling of trafficLights, anchors correctly ─
            ItemDelegate {
                id: backBtn
                anchors.left: trafficLights.right
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                padding: 0
                opacity: window.atHome ? 0 : 1
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 160 } }
                background: Rectangle {
                    radius: 6
                    color: backBtn.hovered ? Theme.color("controlBgHover") : "transparent"
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
                contentItem: Image {
                    anchors.centerIn: parent
                    source: "image://icon/go-previous-symbolic"
                    width: 16; height: 16
                    smooth: true
                }
                onClicked: window.goHome()
                ToolTip.text: qsTr("Back to Settings")
                ToolTip.visible: hovered
                ToolTip.delay: 500
            }

            // ── Module icon + name (centered, module state only) ──────────
            Row {
                anchors.centerIn: parent
                spacing: 8
                opacity: window.atHome ? 0 : 1
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 160 } }

                Image {
                    source: window.currentModule
                        ? "image://icon/" + (window.currentModule.icon || "preferences-system")
                        : ""
                    width: 18; height: 18
                    visible: window.currentModule !== null
                    anchors.verticalCenter: parent.verticalCenter
                    smooth: true
                }
                Text {
                    text: window.currentModule ? (window.currentModule.name || "") : ""
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // ── Search field (right-aligned, home state only) ─────────────
            TextField {
                id: searchField
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                width: 280
                opacity: window.atHome ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 160 } }
                placeholderText: qsTr("Search Settings")
                selectByMouse: true
                leftPadding: 32
                onTextChanged: if (SearchEngine) SearchEngine.searchQuery = text
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "🔍"; font.pixelSize: 13
                    color: Theme.color("textDisabled")
                }
                background: Rectangle {
                    radius: 8
                    color: Theme.color("surface")
                    border.color: searchField.activeFocus ? Theme.color("accent") : Theme.color("panelBorder")
                    border.width: searchField.activeFocus ? 2 : 1
                    Behavior on border.color { ColorAnimation { duration: 120 } }
                }
            }
        }

        // Title bar bottom border
        Rectangle {
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.color("panelBorder")
            z: 15
        }

        // ── Sliding content area ──────────────────────────────────────────
        Item {
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            // Home grid page
            Sidebar {
                id: homePage
                width: parent.width
                height: parent.height
                x: window.atHome ? 0 : -width
                Behavior on x {
                    NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
                }
                moduleModel: ModuleLoader ? ModuleLoader.modules : []
                searchQuery: SearchEngine ? SearchEngine.searchQuery : ""
                onModuleSelected: function(id) { window.openModule(id) }
            }

            // Module detail page
            ContentPanel {
                id: modulePage
                width: parent.width
                height: parent.height
                x: window.atHome ? parent.width : 0
                Behavior on x {
                    NumberAnimation { duration: 300; easing.type: Easing.InOutCubic }
                }
            }
        }

        // ── Debug telemetry ───────────────────────────────────────────────
        Label {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 12; anchors.bottomMargin: 6
            z: 90
            text: SearchEngine
                ? ("search " + SearchEngine.lastSearchLatencyMs + "ms"
                   + " • results " + SearchEngine.lastSearchResultCount
                   + " • module " + (SettingsApp ? SettingsApp.lastModuleOpenLatencyMs : 0) + "ms")
                : ""
            color: Theme.color("textDisabled")
            font.pixelSize: Theme.fontSize("xs")
        }

        // ── Command palette overlay ───────────────────────────────────────
        Rectangle {
            anchors.fill: parent
            color: Theme.color("overlay")
            visible: SettingsApp ? SettingsApp.commandPaletteVisible : false
            z: 100

            MouseArea {
                anchors.fill: parent
                onClicked: if (SettingsApp) SettingsApp.commandPaletteVisible = false
            }

            Pane {
                width: Math.min(parent.width * 0.72, 760)
                height: Math.min(parent.height * 0.62, 520)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top; anchors.topMargin: 80
                z: 101; padding: 14

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    TextField {
                        id: commandSearch
                        Layout.fillWidth: true
                        placeholderText: qsTr("Command palette")
                        text: SearchEngine ? SearchEngine.searchQuery : ""
                        selectByMouse: true
                        onTextChanged: if (SearchEngine) SearchEngine.searchQuery = text
                        Component.onCompleted: forceActiveFocus()
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: SearchEngine ? SearchEngine.results : null
                        currentIndex: 0
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem
                            onClicked: {
                                if (SettingsApp) {
                                    SettingsApp.commandPaletteVisible = false
                                    SettingsApp.openDeepLink(modelData.action)
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 10
                                Image {
                                    source: "image://icon/" + (modelData.icon || "preferences-system")
                                    Layout.preferredWidth: 18; Layout.preferredHeight: 18
                                }
                                ColumnLayout {
                                    spacing: 0
                                    Text { text: modelData.title; font.pixelSize: Theme.fontSize("body"); color: Theme.color("textPrimary") }
                                    Text { text: modelData.subtitle; font.pixelSize: Theme.fontSize("xs"); color: Theme.color("textSecondary") }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
