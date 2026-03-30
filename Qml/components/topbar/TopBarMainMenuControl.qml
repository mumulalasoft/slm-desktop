import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root

    property int iconButtonW: Theme.metric("controlHeightRegular")
    property int iconButtonH: Theme.metric("controlHeightCompact")
    property int iconGlyph: 18
    property var searchProfilesModel: []
    property string activeSearchProfileId: "balanced"
    readonly property bool popupOpen: mainMenu.opened

    signal popupHintRequested()
    signal popupHintCleared()
    signal styleGalleryRequested()
    signal setFontScaleRequested(real scaleValue)
    signal searchProfileSetRequested(string profileId)
    signal searchProfileResetRequested()
    signal refreshSearchProfilesRequested()

    implicitWidth: iconButtonW
    implicitHeight: iconButtonH

    property double lastCloseMs: 0

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    function normalizeProfileId(idValue) {
        var id = String(idValue || "").toLowerCase()
        if (id === "apps-first" || id === "files-first" || id === "balanced") {
            return id
        }
        return "balanced"
    }

    function recentlyClosed(debounceMs) {
        var d = debounceMs === undefined ? 220 : debounceMs
        return (Date.now() - Number(lastCloseMs || 0)) < d
    }

    function openPopup() {
        if (recentlyClosed(180)) {
            return
        }
        root.refreshSearchProfilesRequested()
        root.popupHintRequested()
        Qt.callLater(function() {
            mainMenu.open()
        })
    }

    Rectangle {
        id: mainButton
        anchors.fill: parent
        radius: Theme.radiusControl
        color: mainMouse.containsMouse ? Theme.color("accentSoft") : "transparent"
        Behavior on color {
            enabled: root.microAnimationAllowed()
            ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }

        Image {
            id: mainLogo
            anchors.centerIn: parent
            width: root.iconGlyph
            height: root.iconGlyph
            fillMode: Image.PreserveAspectFit
            source: Theme.darkMode ? "qrc:/icons/logo_white.svg" : "qrc:/icons/logo.svg"
        }

        Label {
            anchors.centerIn: parent
            visible: mainLogo.status !== Image.Ready
            text: "\u25cf"
            color: Theme.color("textOnGlass")
            font.pixelSize: Theme.fontSize("body")
        }

        MouseArea {
            id: mainMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.recentlyClosed(220)) {
                    return
                }
                if (mainMenu.opened || mainMenu.visible) {
                    root.lastCloseMs = Date.now()
                    mainMenu.close()
                } else {
                    root.openPopup()
                }
            }
        }
    }

    Menu {
        id: mainMenu
        modal: false
        focus: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.round(mainButton.mapToGlobal(0, 0).x)
        y: Math.round(mainButton.mapToGlobal(0, 0).y + mainButton.height + Theme.metric("spacingSm"))
        onAboutToShow: root.popupHintCleared()
        onAboutToHide: {
            root.popupHintCleared()
            root.lastCloseMs = Date.now()
        }
        background: DSStyle.PopupSurface {
            implicitWidth: Theme.metric("popupWidthS")
            implicitHeight: 40
            popupOpacity: Theme.popupSurfaceOpacityStrong
        }

        MenuItem {
            text: "About this computer"
        }
        MenuItem {
            text: "Style Gallery"
            font.pixelSize: Theme.fontSize("menu")
            font.weight: Theme.fontWeight("normal")
            onTriggered: root.styleGalleryRequested()
        }

        MenuSeparator {}

        MenuItem {
            text: Theme.darkMode ? "Switch to Light" : "Switch to dark"
            onTriggered: {
                if (Theme.darkMode) {
                    Theme.useLightMode()
                } else {
                    Theme.useDarkMode()
                }
            }
        }

        MenuSeparator {}

        Menu {
            id: textSizeMenu
            title: "Text Size"

            MenuItem {
                text: "Compact (90%)"
                checkable: true
                checked: Math.abs(Theme.userFontScale - 0.9) < 0.001
                onTriggered: root.setFontScaleRequested(0.9)
            }
            MenuItem {
                text: "Default (100%)"
                checkable: true
                checked: Math.abs(Theme.userFontScale - 1.0) < 0.001
                onTriggered: root.setFontScaleRequested(1.0)
            }
            MenuItem {
                text: "Comfortable (110%)"
                checkable: true
                checked: Math.abs(Theme.userFontScale - 1.1) < 0.001
                onTriggered: root.setFontScaleRequested(1.1)
            }
            MenuItem {
                text: "Large (125%)"
                checkable: true
                checked: Math.abs(Theme.userFontScale - 1.25) < 0.001
                onTriggered: root.setFontScaleRequested(1.25)
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Reduced Motion"
            checkable: true
            checked: (typeof UIPreferences !== "undefined"
                      && UIPreferences
                      && UIPreferences.getPreference)
                     ? !!UIPreferences.getPreference("motion.reduced", false) : false
            onToggled: {
                if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                    UIPreferences.setPreference("motion.reduced", checked)
                }
            }
        }

        MenuItem {
            text: "Motion Debug Overlay"
            checkable: true
            checked: (typeof UIPreferences !== "undefined"
                      && UIPreferences
                      && UIPreferences.getPreference)
                     ? !!UIPreferences.getPreference("motion.debugOverlay", false) : false
            onToggled: {
                if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                    UIPreferences.setPreference("motion.debugOverlay", checked)
                }
            }
        }

        Menu {
            id: motionSpeedMenu
            title: "Motion Speed"

            MenuItem {
                text: "Very Slow (0.25x)"
                checkable: true
                checked: (typeof UIPreferences !== "undefined"
                          && UIPreferences
                          && UIPreferences.getPreference)
                         ? Math.abs(Number(UIPreferences.getPreference("motion.timeScale", 1.0)) - 0.25) < 0.001
                         : false
                onTriggered: {
                    if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                        UIPreferences.setPreference("motion.timeScale", 0.25)
                    }
                }
            }

            MenuItem {
                text: "Slow (0.50x)"
                checkable: true
                checked: (typeof UIPreferences !== "undefined"
                          && UIPreferences
                          && UIPreferences.getPreference)
                         ? Math.abs(Number(UIPreferences.getPreference("motion.timeScale", 1.0)) - 0.5) < 0.001
                         : false
                onTriggered: {
                    if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                        UIPreferences.setPreference("motion.timeScale", 0.5)
                    }
                }
            }

            MenuItem {
                text: "Normal (1.00x)"
                checkable: true
                checked: (typeof UIPreferences !== "undefined"
                          && UIPreferences
                          && UIPreferences.getPreference)
                         ? Math.abs(Number(UIPreferences.getPreference("motion.timeScale", 1.0)) - 1.0) < 0.001
                         : true
                onTriggered: {
                    if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                        UIPreferences.setPreference("motion.timeScale", 1.0)
                    }
                }
            }

            MenuItem {
                text: "Fast (1.25x)"
                checkable: true
                checked: (typeof UIPreferences !== "undefined"
                          && UIPreferences
                          && UIPreferences.getPreference)
                         ? Math.abs(Number(UIPreferences.getPreference("motion.timeScale", 1.0)) - 1.25) < 0.001
                         : false
                onTriggered: {
                    if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                        UIPreferences.setPreference("motion.timeScale", 1.25)
                    }
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Dock Motion: Subtle"
            checkable: true
            checked: (typeof UIPreferences !== "undefined"
                      && UIPreferences
                      && UIPreferences.dockMotionPreset !== undefined)
                     ? UIPreferences.dockMotionPreset === "subtle" : true
            onTriggered: {
                if (UIPreferences) {
                    UIPreferences.setDockMotionPreset("subtle")
                }
            }
        }

        MenuItem {
            text: "Dock Motion: macOS Lively"
            checkable: true
            checked: (typeof UIPreferences !== "undefined"
                      && UIPreferences
                      && UIPreferences.dockMotionPreset !== undefined)
                     ? (UIPreferences.dockMotionPreset === "macos-lively"
                        || UIPreferences.dockMotionPreset === "expressive") : false
            onTriggered: {
                if (UIPreferences) {
                    UIPreferences.setDockMotionPreset("macos-lively")
                }
            }
        }

        MenuItem {
            text: "Dock Drop Pulse"
            checkable: true
            checked: (typeof UIPreferences !== "undefined"
                      && UIPreferences
                      && UIPreferences.dockDropPulseEnabled !== undefined)
                     ? UIPreferences.dockDropPulseEnabled : true
            onToggled: {
                if (UIPreferences) {
                    UIPreferences.setDockDropPulseEnabled(checked)
                }
            }
        }

        MenuItem {
            text: "Dock Auto Hide"
            checkable: true
            checked: (typeof UIPreferences !== "undefined"
                      && UIPreferences
                      && UIPreferences.dockAutoHideEnabled !== undefined)
                     ? UIPreferences.dockAutoHideEnabled : false
            onToggled: {
                if (UIPreferences) {
                    UIPreferences.setDockAutoHideEnabled(checked)
                }
            }
        }

        MenuSeparator {}

        Menu {
            id: searchRankingMenu
            title: "Search Ranking"

            Instantiator {
                model: root.searchProfilesModel
                delegate: MenuItem {
                    required property var modelData
                    readonly property string profileId: root.normalizeProfileId(modelData.id)
                    text: String(modelData.label || profileId)
                    checkable: true
                    checked: root.activeSearchProfileId === profileId
                    onTriggered: root.searchProfileSetRequested(profileId)
                }
                onObjectAdded: function(index, object) {
                    searchRankingMenu.insertItem(index, object)
                }
                onObjectRemoved: function(index, object) {
                    searchRankingMenu.removeItem(object)
                }
            }

            MenuSeparator {}

            MenuItem {
                text: "Reset to Balanced"
                onTriggered: root.searchProfileResetRequested()
            }
        }

        MenuSeparator {}
        MenuItem {
            id: sleepAction
            text: "Sleep"
        }

        MenuSeparator {}

        MenuItem {
            id: rebootAction
            text: "Reboot"
        }

        MenuSeparator {}

        MenuItem {
            id: shutdownAction
            text: "Shutdown"
        }
    }
}
