import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

// DeveloperPage — root shell for Settings > Developer.
//
// The secondary navigation pane is always visible so users can see the section
// structure and toggle Developer Mode without navigating to a hidden overview.
// Individual pages handle their own gating for features that require dev mode.

Item {
    id: root
    anchors.fill: parent
    property string highlightSettingId: ""

    // ── Sub-page model ────────────────────────────────────────────────────────

    readonly property var corePages: [
        { id: "overview",          label: "Overview",              icon: "view-grid-symbolic",        group: "" },
        { id: "context-debug",     label: "Context Debug",         icon: "view-list-details-symbolic", group: "" },
        { id: "env-variables",     label: "Environment Variables", icon: "preferences-system-symbolic", group: "Environment" },
        { id: "per-app-overrides", label: "Per-App Overrides",     icon: "application-x-executable",  group: "Environment" },
        { id: "xdg-portals",       label: "XDG Portals",           icon: "security-high-symbolic",    group: "Runtime" },
        { id: "app-sandbox",       label: "App Sandbox",           icon: "security-medium-symbolic",  group: "Runtime" },
        { id: "dbus-inspector",    label: "D-Bus Inspector",       icon: "network-wired-symbolic",    group: "Runtime" },
        { id: "logs",              label: "Logs & Events",         icon: "text-x-log",                group: "Diagnostics" },
        { id: "services",          label: "Process & Services",    icon: "system-run-symbolic",       group: "Diagnostics" },
        { id: "build-info",        label: "Build & Runtime Info",  icon: "help-about-symbolic",       group: "Diagnostics" },
    ]

    readonly property var advancedPages: [
        { id: "ui-inspector",  label: "UI Inspector",    icon: "zoom-in-symbolic",           group: "Inspection" },
        { id: "feature-flags", label: "Feature Flags",   icon: "preferences-other-symbolic", group: "Inspection" },
        { id: "reset",         label: "Reset & Recovery", icon: "edit-undo-symbolic",        group: "Recovery"   },
    ]

    property string currentPageId: "overview"

    function pageForSetting(settingId) {
        var sid = String(settingId || "")
        if (sid === "env-variables" || sid === "path-manager")
            return "env-variables"
        if (sid === "context-debug" || sid === "context-debug-runtime" || sid === "context-debug-raw")
            return "context-debug"
        if (sid === "per-app-overrides")
            return "per-app-overrides"
        if (sid === "logs")
            return "logs"
        if (sid === "services")
            return "services"
        if (sid === "build-info")
            return "build-info"
        if (sid === "reset")
            return "reset"
        if (sid === "xdg-portals")
            return "xdg-portals"
        if (sid === "app-sandbox")
            return "app-sandbox"
        if (sid === "dbus-inspector")
            return "dbus-inspector"
        return "overview"
    }

    function focusSetting(settingId) {
        highlightSettingId = String(settingId || "")
        currentPageId = pageForSetting(highlightSettingId)
        if (pageLoader.item && pageLoader.item.highlightSettingId !== undefined) {
            pageLoader.item.highlightSettingId = highlightSettingId
        }
        if (pageLoader.item && pageLoader.item.focusSetting) {
            pageLoader.item.focusSetting(highlightSettingId)
        }
    }

    onHighlightSettingIdChanged: {
        if (highlightSettingId.length === 0) {
            return
        }
        currentPageId = pageForSetting(highlightSettingId)
        if (pageLoader.item && pageLoader.item.highlightSettingId !== undefined) {
            pageLoader.item.highlightSettingId = highlightSettingId
        }
    }

    // Advanced pages require experimental mode; core pages always shown.
    readonly property var visiblePages: {
        const expMode = DeveloperMode && DeveloperMode.experimentalFeatures
        return expMode ? corePages.concat(advancedPages) : corePages
    }

    // ── Layout ────────────────────────────────────────────────────────────────

    RowLayout {
        anchors.fill: parent
        spacing: 0

        DeveloperNavPane {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            currentPageId: root.currentPageId
            visiblePages: root.visiblePages
            onPageSelected: function(pageId) { root.currentPageId = pageId }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                id: pageLoader
                anchors.fill: parent
                source: root.pageSource(root.currentPageId)
                asynchronous: true
                onLoaded: {
                    if (item && item.highlightSettingId !== undefined) {
                        item.highlightSettingId = root.highlightSettingId
                    }
                    if (item && root.highlightSettingId.length > 0 && item.focusSetting) {
                        item.focusSetting(root.highlightSettingId)
                    }
                }
            }
        }
    }

    // ── Page routing ──────────────────────────────────────────────────────────

    function pageSource(pageId) {
        switch (pageId) {
        case "overview":          return "DeveloperOverviewPage.qml"
        case "context-debug":     return "ContextDebugPage.qml"
        case "env-variables":     return "EnvVariablesPage.qml"
        case "per-app-overrides": return "PerAppOverridesView.qml"
        case "logs":              return "LogsPage.qml"
        case "services":          return "ProcessServicesPage.qml"
        case "build-info":        return "BuildInfoPage.qml"
        case "reset":             return "ResetRecoveryPage.qml"
        case "xdg-portals":       return "XdgPortalsPage.qml"
        case "app-sandbox":       return "AppSandboxPage.qml"
        case "dbus-inspector":    return "DbusInspectorPage.qml"
        case "ui-inspector":      return "StubPage.qml"
        case "feature-flags":     return "FeatureFlagsPage.qml"
        default:                  return "DeveloperOverviewPage.qml"
        }
    }
}
