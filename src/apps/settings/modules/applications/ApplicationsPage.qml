import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight + 48
    clip: true

    property string highlightSettingId: ""

    // ── Default apps data ──────────────────────────────────────────────────────
    readonly property var mimeRows: [
        { settingId: "default-browser",     mime: "x-scheme-handler/http",   label: qsTr("Web Browser"),  desc: qsTr("Used to open web links."),      icon: "applications-internet-symbolic"   },
        { settingId: "default-mail",         mime: "x-scheme-handler/mailto", label: qsTr("Mail"),         desc: qsTr("Used to compose emails."),      icon: "internet-mail-symbolic"           },
        { settingId: "default-image-viewer", mime: "image/jpeg",              label: qsTr("Image Viewer"), desc: qsTr("Used to open image files."),    icon: "image-x-generic-symbolic"         },
        { settingId: "default-video-player", mime: "video/mp4",               label: qsTr("Video Player"), desc: qsTr("Used to open video files."),    icon: "video-x-generic-symbolic"         },
        { settingId: "default-text-editor",  mime: "text/plain",              label: qsTr("Text Editor"),  desc: qsTr("Used to open text files."),     icon: "accessories-text-editor-symbolic" },
        { settingId: "default-file-manager", mime: "inode/directory",         label: qsTr("File Manager"), desc: qsTr("Used to open folders."),        icon: "system-file-manager-symbolic"     }
    ]

    function currentIndexFor(apps, mime) {
        const defaultId = MimeAppsManager.defaultAppForMimeType(mime)
        if (!defaultId) return -1
        for (let i = 0; i < apps.length; ++i)
            if (apps[i].id === defaultId) return i
        return -1
    }

    // ── Startup apps state ─────────────────────────────────────────────────────
    readonly property var startupModel: StartupAppsController ? StartupAppsController.apps : []

    // ── Main layout ────────────────────────────────────────────────────────────
    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        anchors.topMargin: 32
        spacing: 24

        // ── Default Applications ───────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Default Applications")
            Layout.fillWidth: true

            Repeater {
                model: root.mimeRows

                delegate: SettingCard {
                    required property var modelData
                    objectName: modelData.settingId
                    highlighted: root.highlightSettingId === modelData.settingId
                    label: modelData.label
                    description: modelData.desc
                    icon: modelData.icon
                    iconSize: 22
                    Layout.fillWidth: true

                    property var apps: MimeAppsManager ? MimeAppsManager.appsForMimeType(modelData.mime) : []
                    property int selectedIndex: MimeAppsManager ? root.currentIndexFor(apps, modelData.mime) : -1
                    property string selectedAppIcon: {
                        if (selectedIndex < 0 || !apps[selectedIndex]) return ""
                        return String(apps[selectedIndex].icon || "")
                    }

                    RowLayout {
                        spacing: 6

                        // Currently selected app's icon — gives visual feedback without opening dropdown
                        Image {
                            visible: apps.length > 0 && selectedAppIcon.length > 0
                            source: selectedAppIcon.length > 0
                                ? "image://icon/" + selectedAppIcon
                                : ""
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            smooth: true
                            Layout.alignment: Qt.AlignVCenter
                            opacity: 0.80
                        }

                        ComboBox {
                            model: apps.length === 0
                                ? [qsTr("None")]
                                : apps.map(a => a.name)
                            currentIndex: Math.max(0, selectedIndex)
                            enabled: apps.length > 0
                            Layout.preferredWidth: 210
                            onActivated: {
                                MimeAppsManager.setDefaultApp(modelData.mime, apps[currentIndex].id)
                                selectedIndex = currentIndex
                            }
                        }
                    }
                }
            }
        }

        // ── Login Items ────────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Login Items")
            Layout.fillWidth: true

            // Empty state — shown inside the card when no startup apps exist
            Item {
                visible: root.startupModel.length === 0
                Layout.fillWidth: true
                Layout.preferredHeight: 96

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    Image {
                        source: "image://icon/system-run-symbolic"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignHCenter
                        opacity: 0.25
                        smooth: true
                    }
                    Text {
                        text: qsTr("No login items")
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textSecondary")
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: qsTr("Apps you add here will open automatically when you log in.")
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textDisabled")
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.preferredWidth: 280
                    }
                }
            }

            // One SettingCard per startup app
            Repeater {
                model: root.startupModel

                delegate: SettingCard {
                    required property var modelData
                    icon: modelData.icon || "application-x-executable"
                    iconSize: 28
                    label: modelData.name || ""
                    description: modelData.comment || modelData.exec || ""
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: 4

                        // Destructive remove — subtle text button
                        ItemDelegate {
                            implicitHeight: 28
                            padding: 0
                            leftPadding: 8
                            rightPadding: 8

                            contentItem: Text {
                                text: qsTr("Remove")
                                font.pixelSize: Theme.fontSize("small")
                                color: parent.hovered
                                    ? Theme.color("error")
                                    : Theme.color("textSecondary")
                                Behavior on color { ColorAnimation { duration: Theme.durationMicro } }
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? Qt.rgba(1, 0.2, 0.2, 0.07) : "transparent"
                                Behavior on color { ColorAnimation { duration: Theme.durationMicro } }
                            }

                            onClicked: {
                                if (StartupAppsController)
                                    StartupAppsController.removeApp(modelData.id || modelData.exec)
                            }

                            ToolTip.text: qsTr("Remove from login items")
                            ToolTip.visible: hovered
                            ToolTip.delay: 600
                        }

                        Switch {
                            implicitHeight: 26
                            checked: modelData.enabled !== false
                            Layout.alignment: Qt.AlignVCenter
                            onToggled: {
                                if (StartupAppsController)
                                    StartupAppsController.setEnabled(modelData.id || modelData.exec, checked)
                            }
                        }
                    }
                }
            }

            // "Add App…" footer row
            ItemDelegate {
                Layout.fillWidth: true
                implicitHeight: 44
                padding: 0

                background: Rectangle {
                    color: parent.hovered ? Theme.color("controlBgHover") : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.durationSm } }
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    spacing: 8

                    Image {
                        source: "image://icon/list-add-symbolic"
                        Layout.preferredWidth: 15
                        Layout.preferredHeight: 15
                        smooth: true
                        opacity: 0.7
                    }
                    Text {
                        text: qsTr("Add App…")
                        font.pixelSize: Theme.fontSize("body")
                        font.weight: Font.Medium
                        color: Theme.color("accent")
                    }
                    Item { Layout.fillWidth: true }
                }

                onClicked: addStartupDialog.open()
            }
        }

        Item { Layout.preferredHeight: 4 }
    }

    Connections {
        target: MimeAppsManager
        function onDefaultAppChanged(mimeType, desktopFileId) {}
    }

    // ── Add Startup App dialog ─────────────────────────────────────────────────
    Dialog {
        id: addStartupDialog
        modal: true
        anchors.centerIn: parent
        width: 440
        height: 520
        padding: 0
        topPadding: 0; bottomPadding: 0
        leftPadding: 0; rightPadding: 0
        standardButtons: Dialog.NoButton

        background: Rectangle {
            color: Theme.color("windowBg")
            radius: Theme.radiusCard || 8
            border.color: Theme.color("panelBorder")
            border.width: 1
        }

        property var installedApps: []

        onOpened: {
            appSearchField.text = ""
            customCmdField.text = ""
            installedApps = StartupAppsController ? StartupAppsController.installedApps() : []
            appSearchField.forceActiveFocus()
        }

        readonly property var filteredApps: {
            const q = appSearchField.text.trim().toLowerCase()
            const source = addStartupDialog.installedApps || []
            if (q.length === 0) return source
            return source.filter(function(a) {
                if (!a) return false
                return String(a.name || "").toLowerCase().indexOf(q) >= 0
                    || String(a.comment || "").toLowerCase().indexOf(q) >= 0
            })
        }

        function addApp(app) {
            if (StartupAppsController)
                StartupAppsController.addEntry(app.name, app.exec, app.icon, app.comment)
            close()
        }

        function addCustomCommand() {
            const cmd = customCmdField.text.trim()
            if (cmd.length > 0 && StartupAppsController)
                StartupAppsController.addEntry(cmd, cmd, "", "")
            close()
        }

        contentItem: ColumnLayout {
            spacing: 0

            // ── Dialog title bar ──────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 50
                color: Qt.rgba(0.5, 0.5, 0.5, 0.04)

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Choose App to Open at Login")
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    height: 1
                    color: Theme.color("panelBorder")
                }
            }

            // ── Search field ──────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 12; Layout.rightMargin: 12
                Layout.topMargin: 10; Layout.bottomMargin: 2
                height: 34
                radius: 8
                color: Theme.color("surface")
                border.color: appSearchField.activeFocus
                    ? Theme.color("accent") : Theme.color("panelBorder")
                border.width: appSearchField.activeFocus ? 2 : 1

                Behavior on border.color { ColorAnimation { duration: Theme.durationMicro } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10; anchors.rightMargin: 10
                    spacing: 6

                    Image {
                        source: "image://icon/system-search-symbolic"
                        Layout.preferredWidth: 14; Layout.preferredHeight: 14
                        smooth: true
                        opacity: 0.45
                    }

                    TextField {
                        id: appSearchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search")
                        selectByMouse: true
                        background: Item {}
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textPrimary")
                        Keys.onDownPressed: appListView.forceActiveFocus()
                    }
                }
            }

            // ── App list ──────────────────────────────────────────────────────
            ListView {
                id: appListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: addStartupDialog.filteredApps
                ScrollBar.vertical: ScrollBar {}

                // Empty state
                Text {
                    anchors.centerIn: parent
                    visible: appListView.count === 0 && appSearchField.text.length > 0
                    text: qsTr("No results")
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textDisabled")
                }

                delegate: ItemDelegate {
                    width: appListView.width
                    height: 56
                    padding: 0
                    highlighted: ListView.isCurrentItem

                    background: Rectangle {
                        color: parent.highlighted
                            ? Theme.color("accentSoft")
                            : (parent.hovered ? Theme.color("controlBgHover") : "transparent")
                        Behavior on color { ColorAnimation { duration: Theme.durationMicro } }
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16; anchors.rightMargin: 16
                        spacing: 12

                        Image {
                            source: modelData.icon
                                ? "image://icon/" + modelData.icon
                                : "image://icon/application-x-executable"
                            Layout.preferredWidth: 36; Layout.preferredHeight: 36
                            smooth: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: String((modelData && modelData.name) ? modelData.name : "")
                                font.pixelSize: Theme.fontSize("body")
                                color: Theme.color("textPrimary")
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.comment || ""
                                font.pixelSize: Theme.fontSize("small")
                                color: Theme.color("textSecondary")
                                elide: Text.ElideRight
                                visible: text.length > 0
                            }
                        }
                    }

                    onClicked: addStartupDialog.addApp(modelData)

                    Rectangle {
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.bottom: parent.bottom; anchors.leftMargin: 16
                        height: 1
                        color: Theme.color("panelBorder")
                        visible: index < appListView.count - 1
                    }
                }

                Keys.onReturnPressed: {
                    if (currentIndex >= 0 && currentIndex < addStartupDialog.filteredApps.length)
                        addStartupDialog.addApp(addStartupDialog.filteredApps[currentIndex])
                }
                Keys.onUpPressed: {
                    if (currentIndex <= 0) appSearchField.forceActiveFocus()
                    else decrementCurrentIndex()
                }
            }

            // ── Custom command row ────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 46
                color: Theme.color("surface")

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left; anchors.right: parent.right
                    height: 1
                    color: Theme.color("panelBorder")
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14; anchors.rightMargin: 14
                    spacing: 8

                    Text {
                        text: ">_"
                        font.pixelSize: Theme.fontSize("small")
                        font.family: "monospace"
                        color: Theme.color("textDisabled")
                        Layout.alignment: Qt.AlignVCenter
                    }
                    TextField {
                        id: customCmdField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Type in a custom command")
                        selectByMouse: true
                        background: Item {}
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textPrimary")
                        Keys.onReturnPressed: addStartupDialog.addCustomCommand()
                    }
                }
            }

            // ── Footer buttons ────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 52
                color: "transparent"

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left; anchors.right: parent.right
                    height: 1
                    color: Theme.color("panelBorder")
                }

                Button {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Cancel")
                    onClicked: addStartupDialog.close()
                }
            }
        }
    }
}
