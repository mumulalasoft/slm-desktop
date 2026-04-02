import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

// ContentPanel — module detail page.
// Loads the active module's QML via a Loader. The title bar / back button
// live in Main.qml; this component is pure content.

Rectangle {
    id: root
    color: Theme.color("windowBg")

    function applySettingFocus(settingId) {
        const sid = String(settingId || "")
        if (!modulePageLoader.item) return
        if (modulePageLoader.item.highlightSettingId !== undefined)
            modulePageLoader.item.highlightSettingId = sid
        if (sid.length > 0 && modulePageLoader.item.focusSetting) {
            Qt.callLater(function() {
                if (modulePageLoader.item && modulePageLoader.item.focusSetting)
                    modulePageLoader.item.focusSetting(sid)
            })
        }
    }

    // ── Module Loader ─────────────────────────────────────────────────────

    Loader {
        id: modulePageLoader
        anchors.fill: parent
        source: (SettingsApp && SettingsApp.currentModuleId && ModuleLoader)
            ? ModuleLoader.moduleMainQmlUrl(SettingsApp.currentModuleId)
            : ""
        asynchronous: true
        onLoaded: {
            if (SettingsApp) root.applySettingFocus(SettingsApp.currentSettingId)
        }
    }

    Connections {
        target: SettingsApp
        function onDeepLinkOpened(moduleId, settingId) {
            if (moduleId !== (SettingsApp ? SettingsApp.currentModuleId : "")) return
            root.applySettingFocus(settingId)
        }
    }

    // Loading indicator while async Loader is working
    BusyIndicator {
        anchors.centerIn: parent
        running: modulePageLoader.status === Loader.Loading
        visible: running
    }
}
