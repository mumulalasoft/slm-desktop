import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    property var notificationManager: null
    property bool open: false

    signal dismissRequested(int notificationId)
    signal notificationClicked(int notificationId)
    signal closeRequested()

    readonly property int panelWidth: 420

    anchors.fill: parent
    visible: dimmer.opacity > 0.01 || panel.x < width

    function ensureCurrentIndex() {
        if (!centerList || centerList.count <= 0) {
            centerList.currentIndex = -1
            return
        }
        if (centerList.currentIndex < 0 || centerList.currentIndex >= centerList.count) {
            centerList.currentIndex = 0
        }
    }

    Rectangle {
        id: dimmer
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.32)
        opacity: root.open ? 1 : 0
        Behavior on opacity {
            enabled: Theme.animationsEnabled
            NumberAnimation { duration: Theme.durationNormal; easing.type: Theme.easingDefault }
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.open
            onClicked: root.closeRequested()
        }
    }

    Rectangle {
        id: panel
        width: root.panelWidth
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: root.open ? (root.width - width) : root.width
        color: Theme.color("menuBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")

        Behavior on x {
            enabled: Theme.animationsEnabled
            SpringAnimation {
                spring: Theme.physicsSpringGentle
                damping: Theme.physicsDampingGentle
                mass: Theme.physicsMassGentle
                epsilon: Theme.physicsEpsilon
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.notificationCardPadding
            spacing: Theme.notificationVerticalRhythm

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Notification Center")
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("title")
                    font.weight: Theme.fontWeight("semiBold")
                    elide: Text.ElideRight
                }

                Button {
                    text: qsTr("Mark All Read")
                    enabled: root.notificationManager && root.notificationManager.unreadCount > 0
                    onClicked: {
                        if (root.notificationManager && root.notificationManager.markAllRead) {
                            root.notificationManager.markAllRead()
                        }
                    }
                }

                Button {
                    text: qsTr("Clear All")
                    enabled: root.notificationManager && root.notificationManager.count > 0
                    onClicked: {
                        if (root.notificationManager && root.notificationManager.clearAll) {
                            root.notificationManager.clearAll()
                        }
                    }
                }
            }

            ListView {
                id: centerList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.notificationVerticalRhythm
                model: root.notificationManager ? root.notificationManager.notifications : null
                focus: root.open
                keyNavigationWraps: true
                highlightFollowsCurrentItem: true
                highlightMoveDuration: 140
                highlightMoveVelocity: -1
                highlightResizeDuration: 140
                section.property: "appName"
                section.criteria: ViewSection.FullString
                onCountChanged: root.ensureCurrentIndex()
                onModelChanged: root.ensureCurrentIndex()
                onFocusChanged: {
                    if (focus) {
                        root.ensureCurrentIndex()
                    }
                }
                Component.onCompleted: root.ensureCurrentIndex()

                highlight: Rectangle {
                    radius: Theme.radiusWindow
                    color: "transparent"
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("accent")
                    opacity: centerList.activeFocus ? 0.9 : 0.0
                    Behavior on opacity {
                        enabled: Theme.animationsEnabled
                        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                    }
                }

                section.delegate: Rectangle {
                    readonly property int _unreadTick: root.notificationManager
                                                       ? Number(root.notificationManager.unreadCount || 0) : 0
                    readonly property int unreadInSection: {
                        var _tick = _unreadTick
                        if (!root.notificationManager || !root.notificationManager.unreadCountForApp) {
                            return 0
                        }
                        return Number(root.notificationManager.unreadCountForApp(String(section || "")) || 0)
                    }
                    width: centerList.width
                    height: 28
                    radius: Theme.radiusControl
                    color: Theme.color("controlBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: section
                        color: Theme.color("textSecondary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: Theme.fontWeight("semiBold")
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        visible: parent.unreadInSection > 0
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        width: 18
                        height: 18
                        radius: height * 0.5
                        color: Theme.color("accent")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("menuBg")

                        Text {
                            anchors.centerIn: parent
                            text: parent.parent.unreadInSection > 99
                                  ? "99+"
                                  : String(parent.parent.unreadInSection)
                            color: Theme.color("accentText")
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("micro")
                            font.weight: Theme.fontWeight("bold")
                        }
                    }
                }
                delegate: NotificationCard {
                    width: ListView.view ? ListView.view.width : 380
                    appName: String(appName || "")
                    appIcon: String(appIcon || "")
                    title: String(summary || appName || "")
                    body: String(body || "")
                    timestamp: String(timestamp || "")
                    priority: String(priority || "normal")
                    read: !!read
                    actionsModel: actions || []
                    opacity: ListView.isCurrentItem ? 1 : 0.96
                    scale: ListView.isCurrentItem ? 1.0 : 0.992
                    border.color: ListView.isCurrentItem ? Theme.color("accent") : Theme.color("panelBorder")
                    Behavior on opacity {
                        enabled: Theme.animationsEnabled
                        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                    }
                    Behavior on scale {
                        enabled: Theme.animationsEnabled
                        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                    }

                    onClicked: {
                        var id = Number(notificationId || -1)
                        if (id > 0) root.notificationClicked(id)
                    }
                    onDismissRequested: {
                        var id = Number(notificationId || -1)
                        if (id > 0) root.dismissRequested(id)
                    }
                    onActionTriggered: function(actionKey) {
                        var id = Number(notificationId || -1)
                        if (id > 0) {
                            if (root.notificationManager && root.notificationManager.markRead) {
                                root.notificationManager.markRead(id, true)
                            }
                            if (root.notificationManager && root.notificationManager.invokeAction) {
                                root.notificationManager.invokeAction(id, String(actionKey || "default"))
                            }
                            root.closeRequested()
                        }
                    }
                }

                Keys.onPressed: function(event) {
                    if (!root.open) {
                        return
                    }
                    if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                        var row = centerList.currentItem
                        if (row && row.notificationId !== undefined) {
                            root.dismissRequested(Number(row.notificationId))
                            event.accepted = true
                        }
                        return
                    }
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        var rowOpen = centerList.currentItem
                        if (rowOpen && rowOpen.notificationId !== undefined) {
                            root.notificationClicked(Number(rowOpen.notificationId))
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }

    onOpenChanged: {
        if (open) {
            root.ensureCurrentIndex()
            centerList.forceActiveFocus()
        }
    }
}
