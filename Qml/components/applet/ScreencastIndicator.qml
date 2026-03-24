import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

Item {
    id: root

    property var privacyModel: (typeof ScreencastPrivacyModel !== "undefined")
                               ? ScreencastPrivacyModel : null
    readonly property bool active: !!privacyModel && !!privacyModel.active
    readonly property int activeCount: privacyModel ? Number(privacyModel.activeCount) : 0
    readonly property int iconSize: 22
    readonly property bool popupOpen: menu.opened

    visible: active
    implicitWidth: active ? button.implicitWidth : 0
    implicitHeight: button.implicitHeight

    Behavior on opacity {
        NumberAnimation {
            duration: 130
            easing.type: Easing.OutCubic
        }
    }

    ToolButton {
        id: button
        anchors.fill: parent
        enabled: true
        padding: 0
        onClicked: {
            if (menu.opened || menu.visible) {
                menu.close()
            } else {
                menu.open()
            }
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: button.hovered ? Theme.color("accentSoft") : (root.active ? Theme.color("success") : "transparent")
            opacity: button.hovered ? 1.0 : (root.active ? 0.22 : 0.0)
            border.width: Theme.borderWidthThin
            border.color: button.hovered ? Theme.color("panelBorder") : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.durationSm; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: Theme.durationSm; easing.type: Easing.OutCubic } }
        }

        contentItem: Item {
            implicitWidth: root.iconSize
            implicitHeight: root.iconSize

            IconImage {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: "image://themeicon/video-display-symbolic?v=" +
                        ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                         ? ThemeIconController.revision : 0)
                color: Theme.color("textOnGlass")
            }

            Rectangle {
                visible: root.active
                width: 8
                height: 8
                radius: 4
                color: Theme.color("error")
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
            }
        }

        ToolTip.visible: hovered
        ToolTip.delay: 200
        ToolTip.text: root.privacyModel ? root.privacyModel.tooltipText : qsTr("Screen sharing active")
    }

    IndicatorMenu {
        id: menu
        anchorItem: button
        popupGap: Theme.metric("spacingXs")
        popupWidth: Theme.metric("popupWidthM")

        MenuItem {
            enabled: false
            contentItem: RowLayout {
                spacing: Theme.metric("spacingSm")
                Image {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    fillMode: Image.PreserveAspectFit
                    source: "image://themeicon/video-display-symbolic?v=" +
                            ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                             ? ThemeIconController.revision : 0)
                }
                IndicatorSectionLabel {
                    Layout.fillWidth: true
                    text: root.privacyModel ? root.privacyModel.statusTitle : qsTr("Screen sharing is active")
                    emphasized: true
                }
            }
        }

        MenuItem {
            enabled: false
            visible: root.privacyModel && root.privacyModel.statusSubtitle.length > 0
            contentItem: IndicatorSectionLabel {
                text: root.privacyModel ? root.privacyModel.statusSubtitle : ""
                emphasized: false
            }
        }

        MenuItem {
            enabled: false
            visible: root.privacyModel && root.privacyModel.lastActionText.length > 0
            contentItem: IndicatorSectionLabel {
                text: root.privacyModel ? root.privacyModel.lastActionText : ""
                emphasized: false
            }
        }

        MenuSeparator {}

        MenuItem {
            enabled: false
            visible: root.activeCount > 0
            contentItem: IndicatorSectionLabel {
                text: "Active sessions"
                emphasized: true
            }
        }

        Instantiator {
            model: root.privacyModel ? root.privacyModel.activeSessionItems : []
            delegate: MenuItem {
                required property var modelData
                readonly property string sessionPath: modelData && modelData.session ? String(modelData.session) : ""
                readonly property string iconName: modelData && modelData.icon_name
                                                ? String(modelData.icon_name) : "application-x-executable"
                readonly property string labelText: {
                    var label = modelData && modelData.label ? String(modelData.label) : ""
                    if (!label || label.length <= 0) {
                        label = modelData && modelData.app_id ? String(modelData.app_id) : "Unknown app"
                    }
                    return label
                }
                contentItem: RowLayout {
                    spacing: Theme.metric("spacingSm")
                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        fillMode: Image.PreserveAspectFit
                        source: "image://themeicon/" + iconName + "?v=" +
                                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                 ? ThemeIconController.revision : 0)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: labelText
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                    }
                    Text {
                        text: "Stop"
                        color: Theme.color("textSecondary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                    }
                }
                onTriggered: {
                    if (root.privacyModel && root.privacyModel.stopSession(sessionPath)) {
                        if (!root.active) {
                            menu.close()
                        }
                    }
                }
            }
            onObjectAdded: function(index, object) { menu.insertItem(1 + index, object) }
            onObjectRemoved: function(index, object) { menu.removeItem(object) }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Stop all sharing")
            enabled: root.activeCount > 0
            onTriggered: {
                if (!root.privacyModel) {
                    return
                }
                root.privacyModel.stopAllSessions()
                if (!root.active) {
                    menu.close()
                }
            }
        }

        MenuItem {
            enabled: false
            visible: root.activeCount > 0
            contentItem: IndicatorSectionLabel {
                text: qsTr("Select a session to stop sharing immediately.")
            }
        }
    }
}
