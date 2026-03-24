import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

Item {
    id: root

    property var privacyModel: (typeof InputCapturePrivacyModel !== "undefined")
                               ? InputCapturePrivacyModel : null
    readonly property bool active: !!privacyModel && !!privacyModel.active
    readonly property int activeCount: privacyModel ? Number(privacyModel.enabledCount) : 0
    readonly property bool popupOpen: menu.opened
    readonly property int iconSize: 22

    visible: active
    implicitWidth: active ? button.implicitWidth : 0
    implicitHeight: button.implicitHeight

    ToolButton {
        id: button
        anchors.fill: parent
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
            color: button.hovered ? Theme.color("accentSoft") : (root.active ? Theme.color("warning") : "transparent")
            opacity: button.hovered ? 1.0 : (root.active ? 0.2 : 0.0)
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
                source: "image://themeicon/input-mouse-symbolic?v=" +
                        ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                         ? ThemeIconController.revision : 0)
                color: Theme.color("textOnGlass")
            }
            Rectangle {
                visible: root.active
                width: 8
                height: 8
                radius: 4
                color: Theme.color("warning")
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
            }
        }

        ToolTip.visible: hovered
        ToolTip.delay: 200
        ToolTip.text: root.privacyModel ? root.privacyModel.tooltipText : qsTr("Input capture active")
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
                    source: "image://themeicon/input-mouse-symbolic?v=" +
                            ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                             ? ThemeIconController.revision : 0)
                }
                IndicatorSectionLabel {
                    Layout.fillWidth: true
                    text: root.privacyModel ? root.privacyModel.statusTitle : qsTr("Input capture is active")
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

        Instantiator {
            model: root.privacyModel ? root.privacyModel.sessionItems : []
            delegate: MenuItem {
                required property var modelData
                readonly property string sessionPath: modelData && modelData.session_handle ? String(modelData.session_handle) : ""
                readonly property int barrierCount: modelData && modelData.barrier_count ? Number(modelData.barrier_count) : 0
                readonly property bool enabledRow: modelData && !!modelData.enabled
                text: sessionPath
                enabled: enabledRow
                onTriggered: {
                    if (root.privacyModel) {
                        root.privacyModel.disableSession(sessionPath)
                    }
                }
                contentItem: RowLayout {
                    spacing: Theme.metric("spacingSm")
                    IndicatorSectionLabel {
                        Layout.fillWidth: true
                        text: sessionPath
                        emphasized: false
                        elide: Text.ElideMiddle
                    }
                    IndicatorSectionLabel {
                        text: barrierCount + " barriers"
                        emphasized: false
                    }
                    IndicatorSectionLabel {
                        text: enabledRow ? "Disable" : "Idle"
                        emphasized: false
                    }
                }
            }
            onObjectAdded: function(index, object) { menu.insertItem(3 + index, object) }
            onObjectRemoved: function(index, object) { menu.removeItem(object) }
        }
    }
}

