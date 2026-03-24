import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: root
    color: Theme.color("windowBg")

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
                font.pixelSize: Theme.fontSize("hero")
                font.weight: Font.DemiBold
                color: Theme.color("textPrimary")
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
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Font.DemiBold
                            color: Theme.color("textSecondary")
                        }

                        Repeater {
                            model: modelData.entries || []
                            delegate: ItemDelegate {
                                width: parent.width
                                height: 56
                                background: Rectangle {
                                    radius: Theme.radiusCard
                                    color: hovered ? Theme.color("controlBgHover") : Theme.color("surface")
                                    border.color: Theme.color("panelBorder")
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
                                            font.pixelSize: Theme.fontSize("body")
                                            font.weight: Font.Medium
                                            color: Theme.color("textPrimary")
                                        }
                                        Text {
                                            text: modelData.subtitle || ""
                                            font.pixelSize: Theme.fontSize("xs")
                                            color: Theme.color("textSecondary")
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
