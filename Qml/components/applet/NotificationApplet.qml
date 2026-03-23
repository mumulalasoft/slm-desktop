import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

Item {
    id: root

    property var notificationManager: NotificationManager
    property var popupHost: null
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingXs")
    readonly property int rowGap: Theme.metric("spacingMd")
    readonly property bool popupOpen: popupHint || menu.opened

    Timer {
        id: popupHintTimer
        interval: 320
        repeat: false
        onTriggered: root.popupHint = false
    }

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) {
            return
        }
        root.popupHint = true
        popupHintTimer.restart()
        Qt.callLater(function() { menu.open() })
    }

    function loadPreference() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return
        }
        var value = UIPreferences.getPreference("notifications.doNotDisturb", false)
        if (root.notificationManager && root.notificationManager.setDoNotDisturb) {
            root.notificationManager.setDoNotDisturb(!!value)
        }
    }

    function savePreference(enabled) {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.setPreference) {
            return
        }
        UIPreferences.setPreference("notifications.doNotDisturb", !!enabled)
    }

    function notificationCount() {
        if (!root.notificationManager || root.notificationManager.count === undefined || root.notificationManager.count === null) {
            return 0
        }
        return Number(root.notificationManager.count)
    }

    Component.onCompleted: loadPreference()

    ToolButton {
        id: button
        anchors.fill: parent
        padding: 0
        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) {
                return
            }
            if (menu.opened || menu.visible) {
                root.lastMenuCloseMs = Date.now()
                menu.close()
                return
            }
            root.openMenuSafely()
        }

        contentItem: Item {
            implicitWidth: root.iconSize
            implicitHeight: root.iconSize

            Image {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: "image://themeicon/preferences-system-notifications-symbolic?v=" +
                        ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                         ? ThemeIconController.revision : 0)
            }

            Rectangle {
                visible: root.notificationCount() > 0
                anchors.right: parent.right
                anchors.top: parent.top
                width: 14
                height: 14
                radius: Theme.radiusMdPlus
                color: Theme.color("accent")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("menuBg")

                Text {
                    anchors.centerIn: parent
                    text: root.notificationCount() > 9 ? "9+" : String(root.notificationCount())
                    color: Theme.color("accentText")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("micro")
                    font.weight: Theme.fontWeight("bold")
                }
            }
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: button.hovered ? Theme.color("accentSoft") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: button.hovered ? Theme.color("panelBorder") : "transparent"
        }
    }

    IndicatorMenu {
        id: menu
        anchorItem: button
        popupGap: root.popupGap
        popupWidth: Theme.metric("popupWidthL")
        onAboutToShow: root.popupHint = false
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: root.notificationManager && root.notificationManager.serviceRegistered
                      ? "Notifications"
                      : "Notifications (passive mode)"
                emphasized: true
            }
        }

        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: root.notificationCount() > 0
                      ? (root.notificationCount() + " notification(s)")
                      : "No notifications"
            }
        }

        MenuItem {
            contentItem: IndicatorSectionRow {
                text: "Do Not Disturb"
                rowSpacing: root.rowGap
                Switch {
                    checked: root.notificationManager ? root.notificationManager.doNotDisturb : false
                    onToggled: {
                        if (root.notificationManager && root.notificationManager.setDoNotDisturb) {
                            root.notificationManager.setDoNotDisturb(checked)
                        }
                        root.savePreference(checked)
                    }
                }
            }
        }

        MenuSeparator {}

        Instantiator {
            id: items
            model: root.notificationManager ? root.notificationManager.notifications : null
            delegate: MenuItem {
                enabled: true
                contentItem: ColumnLayout {
                    spacing: Theme.metric("spacingXxs")
                    Text {
                        Layout.fillWidth: true
                        text: (summary && summary.length > 0) ? summary : appName
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: Theme.fontWeight("bold")
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: body
                        color: Theme.color("textSecondary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                    }
                }
                onTriggered: {
                    if (root.notificationManager) {
                        root.notificationManager.closeById(notificationId)
                    }
                }
            }
            onObjectAdded: function(index, object) {
                menu.insertItem(3 + index, object)
            }
            onObjectRemoved: function(index, object) {
                menu.removeItem(object)
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Clear All"
            enabled: root.notificationManager && root.notificationManager.count > 0
            onTriggered: {
                if (root.notificationManager) {
                    root.notificationManager.clearAll()
                }
            }
        }
    }
}
