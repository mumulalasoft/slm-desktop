import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property var grants: []
    property var filteredGrants: []
    property var groupedFilteredGrants: []
    property string query: ""

    function reloadGrants() {
        grants = SettingsApp.listSettingGrants()
        applyFilter()
    }

    function applyFilter() {
        const q = String(query || "").trim().toLowerCase()
        if (q.length === 0) {
            filteredGrants = grants
            rebuildGroups()
            return
        }
        const rows = []
        for (let i = 0; i < grants.length; ++i) {
            const g = grants[i]
            const moduleId = String(g.moduleId || "").toLowerCase()
            const settingId = String(g.settingId || "").toLowerCase()
            const decision = String(g.decision || "").toLowerCase()
            const key = moduleId + "/" + settingId
            if (moduleId.indexOf(q) >= 0
                || settingId.indexOf(q) >= 0
                || decision.indexOf(q) >= 0
                || key.indexOf(q) >= 0) {
                rows.push(g)
            }
        }
        filteredGrants = rows
        rebuildGroups()
    }

    function rebuildGroups() {
        const grouped = {}
        for (let i = 0; i < filteredGrants.length; ++i) {
            const row = filteredGrants[i]
            const moduleId = String(row.moduleId || "")
            if (!grouped[moduleId]) {
                grouped[moduleId] = []
            }
            grouped[moduleId].push(row)
        }
        const keys = Object.keys(grouped).sort()
        const list = []
        for (let j = 0; j < keys.length; ++j) {
            const key = keys[j]
            list.push({ moduleId: key, entries: grouped[key] })
        }
        groupedFilteredGrants = list
    }

    Component.onCompleted: reloadGrants()

    onQueryChanged: applyFilter()
    onGrantsChanged: applyFilter()
    onFilteredGrantsChanged: rebuildGroups()

    Connections {
        target: SettingsApp
        function onGrantsChanged() {
            root.reloadGrants()
        }
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("Permission Decisions")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Stored Decisions")
                description: qsTr("Manage cached allow/deny decisions for privileged settings.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    TextField {
                        Layout.preferredWidth: 280
                        placeholderText: qsTr("Filter decisions")
                        text: root.query
                        onTextChanged: root.query = text
                    }
                    Button {
                        text: qsTr("Refresh")
                        onClicked: root.reloadGrants()
                    }
                    Button {
                        text: qsTr("Reset All")
                        enabled: root.grants.length > 0
                        onClicked: {
                            SettingsApp.clearAllSettingGrants()
                            root.reloadGrants()
                        }
                    }
                }
            }

            Repeater {
                model: root.groupedFilteredGrants
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: String(modelData.moduleId || "")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: "#6b7280"
                    }

                    Repeater {
                        model: modelData.entries || []
                        delegate: SettingCard {
                            required property var modelData
                            Layout.fillWidth: true
                            label: {
                                const m = String(modelData.moduleId || "")
                                const s = String(modelData.settingId || "")
                                return m + "/" + s
                            }
                            description: {
                                const decisionText = modelData.decision === "allow-always"
                                                         ? qsTr("Always Allow")
                                                         : qsTr("Always Deny")
                                const updated = String(modelData.updatedAtIso || "")
                                if (updated.length === 0) {
                                    return decisionText
                                }
                                return decisionText + " • " + updated
                            }

                            RowLayout {
                                spacing: 8
                                Label {
                                    text: modelData.decision === "allow-always" ? qsTr("Allow") : qsTr("Deny")
                                    color: modelData.decision === "allow-always" ? "#1f8f4a" : "#b42318"
                                }
                                Button {
                                    text: qsTr("Open Setting")
                                    onClicked: SettingsApp.openModuleSetting(String(modelData.moduleId || ""),
                                                                             String(modelData.settingId || ""))
                                }
                                Button {
                                    text: qsTr("Reset")
                                    onClicked: {
                                        SettingsApp.clearSettingGrant(String(modelData.moduleId || ""),
                                                                      String(modelData.settingId || ""))
                                        root.reloadGrants()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SettingCard {
                visible: root.filteredGrants.length === 0
                Layout.fillWidth: true
                label: root.grants.length === 0
                           ? qsTr("No stored decisions")
                           : qsTr("No matching decisions")
                description: root.grants.length === 0
                                 ? qsTr("Authorization decisions set to Always Allow or Always Deny will appear here.")
                                 : qsTr("Try a different filter query.")
            }
        }
    }
}
