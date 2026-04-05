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
    property string highlightSettingId: ""

    function scrollToItem(item) {
        if (!item) {
            return
        }
        const pt = item.mapToItem(mainCol, 0, 0)
        const target = Math.max(0, Number(pt.y || 0) - 16)
        const maxY = Math.max(0, contentHeight - height)
        contentY = Math.max(0, Math.min(maxY, target))
    }

    function focusSetting(settingId) {
        const sid = String(settingId || "")
        highlightSettingId = sid
        if (sid === "context-debug-raw") {
            scrollToItem(rawContextGroup)
            return
        }
        scrollToItem(runtimeStateGroup)
    }

    function ctxObj() {
        if (typeof ContextClient === "undefined" || !ContextClient || !ContextClient.context) {
            return ({})
        }
        return ContextClient.context || ({})
    }

    function stableStringify(obj) {
        function sortValue(v) {
            if (Array.isArray(v)) {
                var outArr = []
                for (var i = 0; i < v.length; ++i) {
                    outArr.push(sortValue(v[i]))
                }
                return outArr
            }
            if (v && typeof v === "object") {
                var keys = Object.keys(v).sort()
                var out = {}
                for (var j = 0; j < keys.length; ++j) {
                    var k = keys[j]
                    out[k] = sortValue(v[k])
                }
                return out
            }
            return v
        }
        return JSON.stringify(sortValue(obj), null, 2)
    }

    function refreshSnapshot() {
        if (typeof ContextClient !== "undefined" && ContextClient && ContextClient.refresh) {
            ContextClient.refresh()
        }
        contextRawJson.text = root.stableStringify(root.ctxObj())
        const ctx = root.ctxObj()
        const display = ctx.display || {}
        const network = ctx.network || {}
        const attention = ctx.attention || {}
        const activity = ctx.activity || {}
        const system = ctx.system || {}
        sourceSummary.text = "display=" + String(display.fullscreenSource || "-")
                             + " • network=" + String(network.source || "-")
                             + " • attention=" + String(attention.source || "-")
                             + " • activity=" + String(activity.source || "-")
                             + " • system(cpu/mem)=" + String(system.cpuSource || "-")
                             + "/" + String(system.memorySource || "-")
    }

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 16

        Text {
            text: qsTr("Context Debug")
            font.pixelSize: Theme.fontSize("h2")
            font.weight: Theme.fontWeight("semibold")
            color: Theme.color("textPrimary")
        }

        Text {
            text: qsTr("Live read-only context snapshot from org.desktop.Context.")
            font.pixelSize: Theme.fontSize("sm")
            color: Theme.color("textSecondary")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        SettingGroup {
            id: runtimeStateGroup
            Layout.fillWidth: true
            title: qsTr("Runtime State")

            SettingCard {
                width: parent.width
                label: qsTr("Service")
                description: qsTr("org.desktop.Context")
                control: Row {
                    spacing: 6
                    Rectangle {
                        width: 8; height: 8; radius: Theme.radiusSm
                        anchors.verticalCenter: parent.verticalCenter
                        color: (typeof ContextClient !== "undefined" && ContextClient && ContextClient.serviceAvailable)
                               ? (Theme.color("success") || "#22c55e")
                               : Theme.color("textDisabled")
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                        text: (typeof ContextClient !== "undefined" && ContextClient && ContextClient.serviceAvailable)
                              ? qsTr("online") : qsTr("offline")
                    }
                }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Sensor Sources")
                description: sourceSummary.text
                control: Item { width: 1; height: 1 }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Actions")
                description: {
                    const ctx = root.ctxObj()
                    const rules = ctx.rules || {}
                    const actions = rules.actions || {}
                    return "reduceAnimation=" + (!!actions["ui.reduceAnimation"])
                            + " • disableBlur=" + (!!actions["ui.disableBlur"])
                            + " • disableHeavyEffects=" + (!!actions["ui.disableHeavyEffects"])
                }
                control: Item { width: 1; height: 1 }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                spacing: 8
                Button {
                    text: qsTr("Refresh")
                    onClicked: root.refreshSnapshot()
                }
            }
        }

        SettingGroup {
            id: rawContextGroup
            Layout.fillWidth: true
            title: qsTr("Raw Context JSON")

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                radius: Theme.radiusCard
                color: Theme.color("surface")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("panelBorder")

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    TextArea {
                        id: contextRawJson
                        readOnly: true
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.NoWrap
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textPrimary")
                        background: null
                        selectByMouse: true
                        persistentSelection: true
                        text: "{}"
                    }
                }
            }
        }
    }

    Label {
        id: sourceSummary
        visible: false
        text: ""
    }

    Timer {
        id: liveRefreshTimer
        interval: 2000
        repeat: true
        running: true
        onTriggered: root.refreshSnapshot()
    }

    Component.onCompleted: root.refreshSnapshot()
    onHighlightSettingIdChanged: focusSetting(highlightSettingId)
}
