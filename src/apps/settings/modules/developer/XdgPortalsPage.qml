import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainCol.implicitHeight + 48
    clip: true
    property var componentIssues: []
    property bool hasBlockingIssues: false

    function refreshComponentIssues() {
        if (typeof ComponentHealth === "undefined" || !ComponentHealth) {
            componentIssues = []
            hasBlockingIssues = false
            return
        }
        componentIssues = ComponentHealth.missingComponentsForDomain("portal")
        if (ComponentHealth.hasBlockingMissingForDomain) {
            hasBlockingIssues = !!ComponentHealth.hasBlockingMissingForDomain("portal")
        } else {
            hasBlockingIssues = (componentIssues || []).some(function(issue) {
                var level = String((issue || {}).severity || "required").toLowerCase()
                return level === "required"
            })
        }
    }

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 16

        // ── Header ────────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("XDG Portals")
                    font.pixelSize: Theme.fontSize("h2")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    text: qsTr("Configure which portal backend handles each desktop interface.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }

            RowLayout {
                spacing: 8

                Button {
                    text: qsTr("Reset All")
                    enabled: !root.hasBlockingIssues
                    onClicked: resetConfirm.open()
                    ToolTip.text: qsTr("Remove all handler overrides and revert to system defaults")
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                }

                Button {
                    text: qsTr("Refresh")
                    enabled: !root.hasBlockingIssues
                    onClicked: XdgPortals.refresh()
                    ToolTip.text: qsTr("Re-read portal descriptors and portals.conf from disk")
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                }
            }
        }

        // ── Error banner ──────────────────────────────────────────────────────
        MissingComponentsCard {
            Layout.fillWidth: true
            issues: root.componentIssues
            message: qsTr("Portal runtime component missing.")
            installHandler: function(componentId) {
                return ComponentHealth.installComponent(componentId)
            }
            onRefreshRequested: root.refreshComponentIssues()
            onPostInstall: XdgPortals.refresh()
        }

        Rectangle {
            Layout.fillWidth: true
            visible: XdgPortals.lastError.length > 0
            height: errorText.implicitHeight + 16
            radius: 6
            color: Theme.color("errorSubtle")
            border.color: Theme.color("error")
            border.width: 1

            Text {
                id: errorText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                text: XdgPortals.lastError
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("error")
                wrapMode: Text.WordWrap
            }
        }

        // ── Default handler card ──────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: defaultRow.implicitHeight + 24
            radius: 8
            color: Theme.color("panelBg")
            border.color: Theme.color("panelBorder")
            border.width: 1

            RowLayout {
                id: defaultRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: qsTr("Default Fallback")
                        font.pixelSize: Theme.fontSize("sm")
                        font.weight: Font.Medium
                        color: Theme.color("textPrimary")
                    }
                    Text {
                        text: qsTr("Handler used when no interface-specific override is set")
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                    }
                }

                Text {
                    text: XdgPortals.defaultHandler.length > 0
                          ? XdgPortals.defaultHandler
                          : qsTr("(system default)")
                    font.pixelSize: Theme.fontSize("sm")
                    font.family: "monospace"
                    color: XdgPortals.defaultHandler.length > 0
                           ? Theme.color("accent")
                           : Theme.color("textDisabled")
                }
            }
        }

        // ── No descriptors found ──────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            visible: XdgPortals.interfaces.length === 0
            height: 80
            radius: 8
            color: Theme.color("panelBg")
            border.color: Theme.color("panelBorder")
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: qsTr("No portal descriptors found in /usr/share/xdg-desktop-portal/portals/")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textDisabled")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                width: parent.width - 32
            }
        }

        // ── Interface list ────────────────────────────────────────────────────
        Repeater {
            model: XdgPortals.interfaces

            delegate: Rectangle {
                required property string modelData
                readonly property string iface: modelData
                readonly property var ifaceHandlers: XdgPortals.handlersFor(iface)
                readonly property string currentHandler: XdgPortals.preferredHandler(iface)
                readonly property bool hasOverride: XdgPortals.overrides[iface] !== undefined

                Layout.fillWidth: true
                implicitHeight: ifaceCol.implicitHeight + 24
                radius: 8
                color: Theme.color("panelBg")
                border.color: hasOverride
                              ? Theme.color("accent")
                              : Theme.color("panelBorder")
                border.width: hasOverride ? 2 : 1

                ColumnLayout {
                    id: ifaceCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 10

                    // Interface name + override badge
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: {
                                // Strip org.freedesktop.impl.portal. prefix for readability
                                const prefix = "org.freedesktop.impl.portal."
                                return iface.startsWith(prefix)
                                       ? iface.substring(prefix.length)
                                       : iface
                            }
                            font.pixelSize: Theme.fontSize("sm")
                            font.weight: Font.Medium
                            color: Theme.color("textPrimary")
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            visible: hasOverride
                            width: overrideBadge.implicitWidth + 12
                            height: overrideBadge.implicitHeight + 6
                            radius: 4
                            color: Theme.color("accentSubtle")
                            border.color: Theme.color("accent")
                            border.width: 1

                            Text {
                                id: overrideBadge
                                anchors.centerIn: parent
                                text: qsTr("overridden")
                                font.pixelSize: Theme.fontSize("xs")
                                color: Theme.color("accent")
                            }
                        }
                    }

                    // Full interface name (small, muted)
                    Text {
                        Layout.fillWidth: true
                        text: iface
                        font.pixelSize: Theme.fontSize("xs")
                        font.family: "monospace"
                        color: Theme.color("textDisabled")
                        elide: Text.ElideRight
                    }

                    // Handler selector row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("Handler:")
                            font.pixelSize: Theme.fontSize("sm")
                            color: Theme.color("textSecondary")
                        }

                        ComboBox {
                            id: handlerCombo
                            Layout.fillWidth: true
                            enabled: !root.hasBlockingIssues

                            // Build model: "(default)" + handler ids
                            model: {
                                const ids = [qsTr("(default)")]
                                for (let i = 0; i < ifaceHandlers.length; ++i)
                                    ids.push(ifaceHandlers[i].id)
                                return ids
                            }

                            // Select the current override, or "(default)" if none.
                            currentIndex: {
                                const override = XdgPortals.overrides[iface]
                                if (!override) return 0
                                for (let i = 1; i < model.length; ++i) {
                                    if (model[i] === override) return i
                                }
                                return 0
                            }

                            onActivated: index => {
                                if (index === 0)
                                    XdgPortals.setPreferredHandler(iface, "")
                                else
                                    XdgPortals.setPreferredHandler(iface, model[index])
                            }
                        }

                        // Effective handler pill
                        Rectangle {
                            visible: currentHandler.length > 0
                            width: effectiveText.implicitWidth + 12
                            height: effectiveText.implicitHeight + 6
                            radius: 4
                            color: Theme.color("controlBg")
                            border.color: Theme.color("panelBorder")
                            border.width: 1

                            Text {
                                id: effectiveText
                                anchors.centerIn: parent
                                text: currentHandler
                                font.pixelSize: Theme.fontSize("xs")
                                font.family: "monospace"
                                color: Theme.color("textSecondary")
                            }
                        }
                    }

                    // Available handlers chips
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        visible: ifaceHandlers.length > 0

                        Repeater {
                            model: ifaceHandlers

                            delegate: Rectangle {
                                required property var modelData
                                readonly property var handler: modelData

                                width: chipText.implicitWidth + 14
                                height: chipText.implicitHeight + 8
                                radius: 4
                                color: Theme.color("controlBg")
                                border.color: Theme.color("panelBorder")
                                border.width: 1

                                ToolTip.text: handler.dbusName
                                ToolTip.visible: chipHover.containsMouse
                                ToolTip.delay: 400

                                HoverHandler { id: chipHover }

                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: handler.id
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textSecondary")
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Reset confirmation dialog ─────────────────────────────────────────────
    Dialog {
        id: resetConfirm
        title: qsTr("Reset All Portal Handlers?")
        anchors.centerIn: parent
        modal: true

        ColumnLayout {
            spacing: 8
            Text {
                text: qsTr("This will remove all interface-specific handler overrides from\n"
                           + "~/.config/xdg-desktop-portal/portals.conf and revert to\n"
                           + "system-provided defaults.")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textPrimary")
                wrapMode: Text.WordWrap
            }
        }

        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: XdgPortals.resetAllHandlers()
    }

    Component.onCompleted: refreshComponentIssues()
}
