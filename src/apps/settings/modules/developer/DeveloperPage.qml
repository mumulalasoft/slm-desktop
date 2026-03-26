import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

// DeveloperPage — root shell for Settings > Developer.
//
// Renders a secondary sidebar (sub-pages) on the left and loads the selected
// page on the right.  Visibility of pages is gated by DeveloperMode flags.
//
Item {
    id: root
    anchors.fill: parent

    // ── Sub-page model ────────────────────────────────────────────────────
    readonly property var corePages: [
        { id: "overview",          label: "Overview",           icon: "view-grid-symbolic",       group: "" },
        { id: "env-variables",     label: "Environment Variables", icon: "preferences-system-symbolic", group: "Environment" },
        { id: "per-app-overrides", label: "Per-App Overrides",  icon: "application-x-executable", group: "Environment" },
        { id: "xdg-portals",       label: "XDG Portals",        icon: "security-high-symbolic",   group: "Runtime" },
        { id: "app-sandbox",       label: "App Sandbox",        icon: "security-medium-symbolic", group: "Runtime" },
        { id: "dbus-inspector",    label: "D-Bus Inspector",    icon: "network-wired-symbolic",   group: "Runtime" },
        { id: "logs",              label: "Logs & Events",      icon: "text-x-log",               group: "Diagnostics" },
        { id: "services",          label: "Process & Services", icon: "system-run-symbolic",      group: "Diagnostics" },
        { id: "build-info",        label: "Build & Runtime Info", icon: "help-about-symbolic",    group: "Diagnostics" },
    ]

    readonly property var advancedPages: [
        { id: "ui-inspector",  label: "UI Inspector",   icon: "zoom-in-symbolic",         group: "Inspection" },
        { id: "feature-flags", label: "Feature Flags",  icon: "preferences-other-symbolic", group: "Inspection" },
        { id: "reset",         label: "Reset & Recovery", icon: "edit-undo-symbolic",      group: "Recovery"  },
    ]

    property string currentPageId: "overview"

    // The full page list depends on developer mode flags.
    readonly property var visiblePages: {
        const devMode = DeveloperMode && DeveloperMode.developerMode
        const expMode = DeveloperMode && DeveloperMode.experimentalFeatures
        if (!devMode) return []
        let pages = [...corePages]
        if (expMode) pages = pages.concat(advancedPages)
        return pages
    }

    // ── Gate: if developer mode is OFF, show only the toggle ─────────────
    ColumnLayout {
        anchors.fill: parent
        visible: !DeveloperMode || !DeveloperMode.developerMode
        spacing: 0

        SettingCard {
            Layout.fillWidth: true
            label: qsTr("Developer Mode")
            description: qsTr("Enables advanced developer tools including environment management, service inspector, and diagnostics.")
            control: Switch {
                checked: DeveloperMode ? DeveloperMode.developerMode : false
                onToggled: if (DeveloperMode) DeveloperMode.developerMode = checked
            }
        }

        Item { Layout.fillHeight: true }
    }

    // ── Developer mode ON: secondary sidebar + page loader ────────────────
    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: DeveloperMode && DeveloperMode.developerMode

        // Secondary sidebar
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                // Developer Mode toggle at top of secondary sidebar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: qsTr("Developer Mode")
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Font.DemiBold
                        color: Theme.color("textSecondary")
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Switch {
                        implicitHeight: 20
                        checked: DeveloperMode ? DeveloperMode.developerMode : false
                        onToggled: if (DeveloperMode) DeveloperMode.developerMode = checked
                    }
                }

                // Experimental Features toggle (only shown when dev mode is on)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: DeveloperMode && DeveloperMode.developerMode

                    Text {
                        text: qsTr("Experimental")
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Switch {
                        implicitHeight: 20
                        checked: DeveloperMode ? DeveloperMode.experimentalFeatures : false
                        onToggled: if (DeveloperMode) DeveloperMode.experimentalFeatures = checked
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.color("panelBorder")
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                }

                ListView {
                    id: pageList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 1
                    model: root.visiblePages

                    section.property: "group"
                    section.criteria: ViewSection.FullString
                    section.delegate: Item {
                        width: pageList.width
                        height: modelData ? 22 : 0
                        visible: modelData.length > 0
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: section
                            font.pixelSize: Theme.fontSize("xs")
                            font.weight: Font.DemiBold
                            color: Theme.color("textDisabled")
                            visible: section.length > 0
                        }
                    }

                    delegate: ItemDelegate {
                        width: pageList.width
                        height: 30
                        highlighted: root.currentPageId === modelData.id

                        background: Rectangle {
                            radius: 5
                            color: highlighted
                                ? Theme.color("accent")
                                : (hovered ? Theme.color("controlBgHover") : "transparent")
                            Behavior on color {
                                ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 6
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            anchors.left: parent.left
                            anchors.right: parent.right

                            Image {
                                source: "image://icon/" + modelData.icon
                                Layout.preferredWidth: 14
                                Layout.preferredHeight: 14
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.label
                                font.pixelSize: Theme.fontSize("sm")
                                color: highlighted ? Theme.color("accentText") : Theme.color("textPrimary")
                                elide: Text.ElideRight
                                Behavior on color {
                                    ColorAnimation { duration: 100 }
                                }
                            }
                        }

                        onClicked: root.currentPageId = modelData.id
                    }
                }
            }
        }

        // Page content
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                id: pageLoader
                anchors.fill: parent
                source: root.pageSource(root.currentPageId)
                asynchronous: true
            }
        }
    }

    function pageSource(pageId) {
        switch (pageId) {
        case "overview":          return "DeveloperOverviewPage.qml"
        case "env-variables":     return "EnvVariablesPage.qml"
        case "per-app-overrides": return "PerAppOverridesView.qml"
        case "logs":              return "LogsPage.qml"
        case "services":          return "ProcessServicesPage.qml"
        case "build-info":        return "BuildInfoPage.qml"
        case "reset":             return "ResetRecoveryPage.qml"
        // Stubs for future pages
        case "xdg-portals":       return "StubPage.qml"
        case "app-sandbox":       return "StubPage.qml"
        case "dbus-inspector":    return "StubPage.qml"
        case "ui-inspector":      return "StubPage.qml"
        case "feature-flags":     return "FeatureFlagsPage.qml"
        default:                  return "DeveloperOverviewPage.qml"
        }
    }
}
