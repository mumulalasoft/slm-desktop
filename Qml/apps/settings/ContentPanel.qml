import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#f4f5f7"

    property alias query: localSearchField.text
    signal queryChangedByUser(string text)

    function moduleInfo(id) {
        return ModuleLoader.moduleById(id)
    }

    function applySettingFocus(settingId) {
        var sid = String(settingId || "")
        if (!modulePageLoader.item) {
            return
        }
        if (modulePageLoader.item.highlightSettingId !== undefined) {
            modulePageLoader.item.highlightSettingId = sid
        }
        if (sid.length > 0 && modulePageLoader.item.focusSetting) {
            Qt.callLater(function() {
                if (modulePageLoader.item && modulePageLoader.item.focusSetting) {
                    modulePageLoader.item.focusSetting(sid)
                }
            })
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: "\u2039"
                enabled: SettingsApp.recentHistory.length > 1
                onClicked: SettingsApp.back()
            }

            Label {
                Layout.fillWidth: true
                text: SettingsApp.breadcrumb.length > 0 ? SettingsApp.breadcrumb : qsTr("System Settings")
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: "#111827"
                elide: Text.ElideRight
            }

            TextField {
                id: localSearchField
                Layout.preferredWidth: 300
                placeholderText: qsTr("Search in settings")
                selectByMouse: true
                onTextChanged: root.queryChangedByUser(text)
            }
        }

        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: localSearchField.text.trim().length > 0 ? 0 : 1

            // Search mode
            Pane {
                opacity: stack.currentIndex === 0 ? 1.0 : 0.0
                y: stack.currentIndex === 0 ? 0 : 8
                padding: 0
                background: Rectangle { color: "transparent" }
                Behavior on opacity {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }
                Behavior on y {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }

                ListView {
                    anchors.fill: parent
                    clip: true
                    spacing: 6
                    model: SearchEngine.groupedResults

                    delegate: Column {
                        width: ListView.view.width
                        spacing: 4

                        Label {
                            text: modelData.group
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            color: "#6b7280"
                        }

                        Repeater {
                            model: modelData.entries || []
                            delegate: ItemDelegate {
                                width: parent.width
                                height: 56
                                background: Rectangle {
                                    radius: 10
                                    color: hovered ? "#e6edf8" : "#ffffff"
                                    border.color: "#d9e0ea"
                                    border.width: 1
                                }
                                onClicked: SettingsApp.openDeepLink(modelData.action || "")
                                contentItem: RowLayout {
                                    spacing: 10
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12

                                    Image {
                                        source: "image://icon/" + (modelData.icon || "preferences-system")
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            text: modelData.title || ""
                                            font.pixelSize: 14
                                            font.weight: Font.Medium
                                            color: "#111827"
                                        }
                                        Text {
                                            text: modelData.subtitle || ""
                                            font.pixelSize: 11
                                            color: "#6b7280"
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Module mode
            Pane {
                opacity: stack.currentIndex === 1 ? 1.0 : 0.0
                y: stack.currentIndex === 1 ? 0 : 8
                padding: 0
                background: Rectangle { color: "transparent" }
                Behavior on opacity {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }
                Behavior on y {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }

                Loader {
                    id: modulePageLoader
                    anchors.fill: parent
                    source: ModuleLoader.moduleMainQmlUrl(SettingsApp.currentModuleId)
                    asynchronous: true
                    onLoaded: {
                        root.applySettingFocus(SettingsApp.currentSettingId)
                    }
                }

                Connections {
                    target: SettingsApp
                    function onDeepLinkOpened(moduleId, settingId) {
                        if (moduleId !== SettingsApp.currentModuleId) {
                            return
                        }
                        root.applySettingFocus(settingId)
                    }
                }
            }
        }
    }
}
