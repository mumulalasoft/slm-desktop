import QtQuick
import QtQuick.Controls
import Slm_Desktop
import SlmStyle as DSStyle

// GlobalMenuAppIdentity — app icon + name button with a dropdown containing
// system-level app actions (About, Settings, Quit, etc.).
//
// Always reads the focused app from AppStateClient/GlobalMenuManager.

Item {
    id: root

    signal requestAbout()
    signal requestSettings()
    signal requestShortcuts()
    signal requestPermissions()
    signal requestRestartApp()
    signal requestQuitApp()

    // ── resolved app info ─────────────────────────────────────────────────────
    readonly property string _appId: {
        if (typeof AppStateClient !== "undefined" && AppStateClient && AppStateClient.focusedAppId)
            return AppStateClient.focusedAppId
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager)
            return GlobalMenuManager.activeAppId
        return ""
    }

    readonly property var _appData: {
        if (typeof AppStateClient !== "undefined" && AppStateClient
                && _appId.length > 0 && AppStateClient.getApp)
            return AppStateClient.getApp(_appId) || {}
        return {}
    }

    readonly property string _appName: {
        var n = String(_appData.name || _appId || "")
        if (n.length > 0) return n
        return "Desktop"
    }

    readonly property string _appIcon: String(_appData.icon || _appData.iconName || "")

    implicitWidth: identityRow.implicitWidth + Theme.metric("spacingMd") * 2
    implicitHeight: parent ? parent.height : 28

    // ── layout ────────────────────────────────────────────────────────────────
    Row {
        id: identityRow
        anchors.centerIn: parent
        spacing: Theme.metric("spacingXs")

        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 16; height: 16
            source: root._appIcon.length > 0
                    ? (root._appIcon.startsWith("qrc:")
                       || root._appIcon.startsWith("file:")
                       || root._appIcon.startsWith("/")
                       ? root._appIcon
                       : "image://theme/" + root._appIcon)
                    : ""
            visible: source.toString().length > 0
            fillMode: Image.PreserveAspectFit
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root._appName
            color: Theme.color("textOnGlass")
            font.pixelSize: Theme.fontSize("bodyLarge")
            font.weight: Theme.fontWeight("semibold")
            maximumLineCount: 1
            elide: Text.ElideRight
            width: Math.min(implicitWidth, 130)
        }

        // Chevron
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "\u203a"   // ›
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("bodySmall")
            opacity: Theme.opacityMuted
            rotation: identityDropdown.visible ? 90 : 0
            Behavior on rotation {
                NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
        }
    }

    // ── hover + tap ────────────────────────────────────────────────────────────
    HoverHandler { id: hov }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSm
        color: Theme.color("accentSoft")
        opacity: (hov.hovered || identityDropdown.visible) ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }
    }

    TapHandler {
        onTapped: identityDropdown.visible ? identityDropdown.close() : identityDropdown.open()
    }

    // ── dropdown ───────────────────────────────────────────────────────────────
    GlobalMenuDropdown {
        id: identityDropdown
        x: 0
        y: root.height + Theme.metric("spacingXs")

        menuId: 1000  // synthetic AppIdentity menu ID
        menuItems: {
            var name = root._appName
            var items = [
                { id: 1, label: "About " + name,      enabled: true, separator: false },
                { id: 2, label: "Settings",            enabled: true, separator: false },
                { id: 3, label: "Keyboard Shortcuts",  enabled: true, separator: false },
                { id: -1, separator: true },
                { id: 4, label: "Permissions",         enabled: true, separator: false },
                { id: -2, separator: true },
                { id: 5, label: "Restart App",         enabled: root._appId.length > 0, separator: false },
                { id: 6, label: "Quit " + name,        enabled: root._appId.length > 0, separator: false }
            ]
            return items
        }

        onItemActivated: function(mId, itemId) {
            switch (itemId) {
            case 1: root.requestAbout();       break
            case 2: root.requestSettings();    break
            case 3: root.requestShortcuts();   break
            case 4: root.requestPermissions(); break
            case 5: root.requestRestartApp();  break
            case 6: root.requestQuitApp();     break
            }
        }
    }
}
