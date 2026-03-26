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

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Build & Runtime Info")
                    font.pixelSize: Theme.fontSize("h2")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    text: qsTr("Read-only snapshot of build configuration and runtime environment.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                }
            }

            Button {
                text: qsTr("Copy All")
                onClicked: {
                    if (BuildInfo) {
                        clipboard.text = BuildInfo.allAsText()
                    }
                }
                ToolTip.text: qsTr("Copy all info as plain text")
                ToolTip.visible: hovered
                ToolTip.delay: 600
            }
        }

        // Invisible clipboard helper
        TextEdit {
            id: clipboard
            visible: false
            onTextChanged: {
                selectAll()
                copy()
            }
        }

        // ── Info groups ───────────────────────────────────────────────────
        Repeater {
            model: groupedSections()

            delegate: SettingGroup {
                Layout.fillWidth: true
                label: modelData.group

                Column {
                    width: parent.width
                    spacing: 0

                    Repeater {
                        model: modelData.entries
                        delegate: Rectangle {
                            width: parent.width
                            height: 36
                            color: index % 2 === 0 ? "transparent" : Qt.rgba(0, 0, 0, 0.02)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12

                                Text {
                                    Layout.preferredWidth: 180
                                    text: modelData.key
                                    font.pixelSize: Theme.fontSize("sm")
                                    color: Theme.color("textSecondary")
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.value
                                    font.pixelSize: Theme.fontSize("sm")
                                    font.family: "monospace"
                                    color: Theme.color("textPrimary")
                                    elide: Text.ElideRight

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.IBeamCursor
                                        onClicked: {
                                            clipboard.text = modelData.value
                                        }
                                        ToolTip.text: qsTr("Click to copy")
                                        ToolTip.visible: containsMouse
                                        ToolTip.delay: 500
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function groupedSections() {
        if (!BuildInfo) return []
        const sections = BuildInfo.sections
        const groups = {}
        const order = []
        for (const s of sections) {
            if (!groups[s.group]) {
                groups[s.group] = []
                order.push(s.group)
            }
            groups[s.group].push({ key: s.key, value: s.value })
        }
        return order.map(g => ({ group: g, entries: groups[g] }))
    }
}
