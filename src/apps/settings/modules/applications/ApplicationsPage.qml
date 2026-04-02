import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Item {
    id: root
    anchors.fill: parent

    property string highlightSettingId: ""

    // ── Default apps helpers ───────────────────────────────────────────────

    readonly property var mimeRows: [
        { settingId: "default-browser",     mime: "x-scheme-handler/http",   label: qsTr("Web Browser"),   desc: qsTr("Used to open web links.")     },
        { settingId: "default-mail",         mime: "x-scheme-handler/mailto", label: qsTr("Mail"),          desc: qsTr("Used to compose emails.")     },
        { settingId: "default-image-viewer", mime: "image/jpeg",              label: qsTr("Image Viewer"),  desc: qsTr("Used to open image files.")   },
        { settingId: "default-video-player", mime: "video/mp4",               label: qsTr("Video Player"),  desc: qsTr("Used to open video files.")   },
        { settingId: "default-text-editor",  mime: "text/plain",              label: qsTr("Text Editor"),   desc: qsTr("Used to open text files.")    },
        { settingId: "default-file-manager", mime: "inode/directory",         label: qsTr("File Manager"),  desc: qsTr("Used to open folders.")       }
    ]

    function currentIndexFor(apps, mime) {
        const defaultId = MimeAppsManager.defaultAppForMimeType(mime)
        if (!defaultId) return -1
        for (let i = 0; i < apps.length; ++i)
            if (apps[i].id === defaultId) return i
        return -1
    }

    // ── Sidebar nav ────────────────────────────────────────────────────────

    readonly property var pages: [
        { id: "defaults", label: qsTr("Defaults"), icon: "preferences-system-symbolic" },
        { id: "startup",  label: qsTr("Startup"),  icon: "system-run-symbolic"          },
    ]

    property int currentIndex: 0

    // ── Layout ─────────────────────────────────────────────────────────────

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Left sidebar ───────────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

                // System section
                Item { Layout.fillWidth: true; height: 6 }

                Text {
                    text: qsTr("System")
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    color: Theme.color("textDisabled")
                    Layout.leftMargin: 6
                    Layout.bottomMargin: 2
                }

                Repeater {
                    model: root.pages
                    delegate: ItemDelegate {
                        Layout.fillWidth: true
                        height: 34
                        padding: 0
                        highlighted: root.currentIndex === index

                        background: Rectangle {
                            radius: 6
                            color: highlighted
                                ? Theme.color("accent")
                                : (parent.hovered ? Theme.color("controlBgHover") : "transparent")
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }

                        contentItem: RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            spacing: 8

                            Image {
                                source: "image://icon/" + modelData.icon
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                smooth: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.label
                                font.pixelSize: Theme.fontSize("sm")
                                font.weight: highlighted ? Font.DemiBold : Font.Normal
                                color: highlighted
                                    ? Theme.color("accentText")
                                    : Theme.color("textPrimary")
                                elide: Text.ElideRight
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                        }

                        onClicked: root.currentIndex = index
                    }
                }

                // Separator
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.color("panelBorder")
                    Layout.topMargin: 6
                    Layout.bottomMargin: 4
                }

                // Apps section header
                Text {
                    text: qsTr("Apps")
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    color: Theme.color("textDisabled")
                    Layout.leftMargin: 6
                    Layout.bottomMargin: 2
                }

                // Installed apps list
                ListView {
                    id: appList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 1
                    model: MimeAppsManager ? MimeAppsManager.installedApps : []

                    delegate: ItemDelegate {
                        width: appList.width
                        height: 44
                        padding: 0

                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? Theme.color("controlBgHover") : "transparent"
                            Behavior on color { ColorAnimation { duration: 100 } }
                        }

                        contentItem: RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 6
                            spacing: 8

                            Image {
                                source: modelData.icon
                                    ? "image://icon/" + modelData.icon
                                    : "image://icon/application-x-executable"
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                smooth: true
                                Layout.alignment: Qt.AlignVCenter
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name || ""
                                    font.pixelSize: Theme.fontSize("sm")
                                    font.weight: Font.Medium
                                    color: Theme.color("textPrimary")
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: (modelData.categories || []).join(", ")
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textDisabled")
                                    elide: Text.ElideRight
                                    visible: (modelData.categories || []).length > 0
                                }
                            }
                        }
                    }

                    // Empty state — no installed apps exposed yet
                    Text {
                        anchors.centerIn: parent
                        visible: appList.count === 0
                        text: qsTr("No apps")
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textDisabled")
                    }
                }
            }
        }

        // ── Right content ──────────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentIndex

            // ── 0: Default Apps ────────────────────────────────────────────
            Flickable {
                contentHeight: defaultsCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: defaultsCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24
                    anchors.topMargin: 24
                    spacing: 20

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
                                Layout.fillWidth: true

                                property var apps: MimeAppsManager.appsForMimeType(modelData.mime)
                                property int selectedIndex: root.currentIndexFor(apps, modelData.mime)

                                RowLayout {
                                    spacing: 8
                                    Layout.fillWidth: true

                                    ComboBox {
                                        model: apps.length === 0
                                            ? [qsTr("No apps found")]
                                            : apps.map(a => a.name)
                                        currentIndex: Math.max(0, selectedIndex)
                                        enabled: apps.length > 0
                                        Layout.preferredWidth: 240
                                        onActivated: {
                                            MimeAppsManager.setDefaultApp(modelData.mime, apps[currentIndex].id)
                                            selectedIndex = currentIndex
                                        }
                                    }

                                    Label {
                                        text: MimeAppsManager.defaultAppForMimeType(modelData.mime) || ""
                                        color: Theme.color("textDisabled")
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }

                Connections {
                    target: MimeAppsManager
                    function onDefaultAppChanged(mimeType, desktopFileId) {}
                }
            }

            // ── 1: Startup Apps ────────────────────────────────────────────
            Item {
                id: startupPage
                // Startup app model — backed by StartupAppsController if available
                property var startupModel: StartupAppsController ? StartupAppsController.apps : []

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16

                    // Page title row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Image {
                            source: "image://icon/system-run-symbolic"
                            width: 36; height: 36
                            smooth: true
                        }
                        Text {
                            text: qsTr("Startup")
                            font.pixelSize: Theme.fontSize("title") || 22
                            font.weight: Font.DemiBold
                            color: Theme.color("textPrimary")
                        }
                    }

                    // App list card
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Theme.radiusCard || 8
                        color: Theme.color("surface")
                        border.color: Theme.color("panelBorder")
                        border.width: 1
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Empty state
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                visible: startupListView.count === 0

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 8

                                    Image {
                                        source: "image://icon/system-run-symbolic"
                                        Layout.preferredWidth: 40
                                        Layout.preferredHeight: 40
                                        Layout.alignment: Qt.AlignHCenter
                                        opacity: 0.3
                                        smooth: true
                                    }
                                    Text {
                                        text: qsTr("No startup apps configured")
                                        font.pixelSize: Theme.fontSize("body")
                                        color: Theme.color("textSecondary")
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    Text {
                                        text: qsTr("Apps added here will launch automatically when you log in.")
                                        font.pixelSize: Theme.fontSize("sm")
                                        color: Theme.color("textDisabled")
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.alignment: Qt.AlignHCenter
                                        wrapMode: Text.WordWrap
                                        Layout.preferredWidth: 300
                                    }
                                }
                            }

                            // Startup entries list
                            ListView {
                                id: startupListView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                model: startupPage.startupModel
                                visible: count > 0

                                delegate: Item {
                                    width: startupListView.width
                                    height: 60

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 14
                                        anchors.rightMargin: 14
                                        spacing: 12

                                        // App icon
                                        Image {
                                            source: modelData.icon
                                                ? "image://icon/" + modelData.icon
                                                : "image://icon/application-x-executable"
                                            Layout.preferredWidth: 36
                                            Layout.preferredHeight: 36
                                            smooth: true
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        // Name + description
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name || ""
                                                font.pixelSize: Theme.fontSize("body")
                                                font.weight: Font.Medium
                                                color: Theme.color("textPrimary")
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.comment || modelData.exec || ""
                                                font.pixelSize: Theme.fontSize("sm")
                                                color: Theme.color("textSecondary")
                                                elide: Text.ElideRight
                                            }
                                        }

                                        // Delete button
                                        ToolButton {
                                            icon.name: "edit-delete-symbolic"
                                            icon.width: 16; icon.height: 16
                                            implicitWidth: 30; implicitHeight: 30
                                            padding: 0
                                            opacity: 0.6
                                            Layout.alignment: Qt.AlignVCenter
                                            onClicked: {
                                                if (StartupAppsController)
                                                    StartupAppsController.removeApp(modelData.id || modelData.exec)
                                            }
                                            background: Rectangle {
                                                radius: 6
                                                color: parent.hovered
                                                    ? Theme.color("destructiveSubtle") || Qt.rgba(1,0,0,0.08)
                                                    : "transparent"
                                            }
                                            ToolTip.text: qsTr("Remove from startup")
                                            ToolTip.visible: hovered
                                            ToolTip.delay: 500
                                        }

                                        // Enable toggle
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

                                    // Row divider
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.leftMargin: 14
                                        height: 1
                                        color: Theme.color("panelBorder")
                                        visible: index < startupListView.count - 1
                                    }
                                }
                            }

                            // "Add Startup App..." footer
                            Rectangle {
                                Layout.fillWidth: true
                                height: 44
                                color: Theme.color("windowBg")
                                border.color: Theme.color("panelBorder")

                                // Top border only
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: 1
                                    color: Theme.color("panelBorder")
                                }

                                ItemDelegate {
                                    anchors.fill: parent
                                    padding: 0

                                    background: Rectangle {
                                        color: parent.hovered
                                            ? Theme.color("controlBgHover")
                                            : "transparent"
                                        Behavior on color { ColorAnimation { duration: 100 } }
                                    }

                                    contentItem: RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        spacing: 6

                                        Image {
                                            source: "image://icon/list-add-symbolic"
                                            Layout.preferredWidth: 14
                                            Layout.preferredHeight: 14
                                            smooth: true
                                            opacity: 0.7
                                        }
                                        Text {
                                            text: qsTr("Add Startup App…")
                                            font.pixelSize: Theme.fontSize("sm")
                                            color: Theme.color("textSecondary")
                                        }
                                        Item { Layout.fillWidth: true }
                                    }

                                    onClicked: {
                                        addStartupDialog.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Add Startup App dialog ─────────────────────────────────────────────
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

        // Lazy-load the app list when the dialog opens
        property var appList: []
        onOpened: {
            appSearchField.text = ""
            customCmdField.text = ""
            if (StartupAppsController)
                appList = StartupAppsController.installedApps()
            appSearchField.forceActiveFocus()
        }

        // Filtered model
        readonly property var filteredApps: {
            const q = appSearchField.text.trim().toLowerCase()
            if (q.length === 0) return appList
            return appList.filter(function(a) {
                return a.name.toLowerCase().indexOf(q) >= 0
                    || (a.comment && a.comment.toLowerCase().indexOf(q) >= 0)
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

            // ── Search field ──────────────────────────────────────────────
            TextField {
                id: appSearchField
                Layout.fillWidth: true
                Layout.margins: 12
                placeholderText: qsTr("Search Applications")
                selectByMouse: true
                leftPadding: 32
                background: Rectangle {
                    radius: 8
                    color: Theme.color("surface")
                    border.color: appSearchField.activeFocus
                        ? Theme.color("accent") : Theme.color("panelBorder")
                    border.width: appSearchField.activeFocus ? 2 : 1
                }
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "🔍"; font.pixelSize: 13
                    color: Theme.color("textDisabled")
                }
                Keys.onDownPressed: appListView.forceActiveFocus()
            }

            // ── App list ──────────────────────────────────────────────────
            ListView {
                id: appListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: addStartupDialog.filteredApps

                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    width: appListView.width
                    height: 56
                    padding: 0
                    highlighted: ListView.isCurrentItem

                    background: Rectangle {
                        color: parent.hovered ? Theme.color("controlBgHover") : "transparent"
                        Behavior on color { ColorAnimation { duration: 80 } }
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
                                text: modelData.name
                                font.pixelSize: Theme.fontSize("body")
                                color: Theme.color("textPrimary")
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.comment || ""
                                font.pixelSize: Theme.fontSize("sm")
                                color: Theme.color("textSecondary")
                                elide: Text.ElideRight
                                visible: text.length > 0
                            }
                        }
                    }

                    onClicked: addStartupDialog.addApp(modelData)

                    // Row divider
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

            // ── Custom command row ────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 46
                color: Theme.color("surface")

                // Top separator
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left; anchors.right: parent.right
                    height: 1
                    color: Theme.color("panelBorder")
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12; anchors.rightMargin: 12
                    spacing: 8

                    Text {
                        text: ">_"
                        font.pixelSize: Theme.fontSize("sm")
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
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textPrimary")
                        Keys.onReturnPressed: addStartupDialog.addCustomCommand()
                    }
                }
            }

            // ── Cancel button row ─────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 52
                color: "transparent"

                // Top separator
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
