import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property string highlightSettingId: ""

    // Each row is described by a simple object so the repeater stays DRY.
    readonly property var mimeRows: [
        { settingId: "default-browser",      mime: "x-scheme-handler/http",  label: qsTr("Web Browser"),     desc: qsTr("Used to open web links.") },
        { settingId: "default-mail",          mime: "x-scheme-handler/mailto",label: qsTr("Mail Application"),desc: qsTr("Used to compose emails.") },
        { settingId: "default-image-viewer",  mime: "image/jpeg",             label: qsTr("Image Viewer"),    desc: qsTr("Used to open image files.") },
        { settingId: "default-video-player",  mime: "video/mp4",              label: qsTr("Video Player"),    desc: qsTr("Used to open video files.") },
        { settingId: "default-text-editor",   mime: "text/plain",             label: qsTr("Text Editor"),     desc: qsTr("Used to open text files.") },
        { settingId: "default-file-manager",  mime: "inode/directory",        label: qsTr("File Manager"),    desc: qsTr("Used to open folders.") }
    ]

    function currentIndexFor(apps, mime) {
        const defaultId = MimeAppsManager.defaultAppForMimeType(mime)
        if (!defaultId) return -1
        for (let i = 0; i < apps.length; ++i) {
            if (apps[i].id === defaultId) return i
        }
        return -1
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
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

                    // Fetch app list once at creation; updates on defaultAppChanged.
                    property var apps: MimeAppsManager.appsForMimeType(modelData.mime)
                    property int selectedIndex: root.currentIndexFor(apps, modelData.mime)

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true

                        ComboBox {
                            id: combo
                            model: {
                                if (parent.parent.apps.length === 0)
                                    return [qsTr("No apps found")]
                                return parent.parent.apps.map(a => a.name)
                            }
                            currentIndex: Math.max(0, parent.parent.selectedIndex)
                            enabled: parent.parent.apps.length > 0
                            Layout.preferredWidth: 240

                            onActivated: {
                                const row = parent.parent.parent // SettingCard content
                                MimeAppsManager.setDefaultApp(
                                    modelData.mime,
                                    row.apps[currentIndex].id)
                                row.selectedIndex = currentIndex
                            }
                        }

                        // Show the current desktop-file ID as a subtle hint
                        Label {
                            text: {
                                const id = MimeAppsManager.defaultAppForMimeType(modelData.mime)
                                return id ? id : ""
                            }
                            color: "#9ca3af"
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
        function onDefaultAppChanged(mimeType, desktopFileId) {
            // Force the repeater to refresh its current-index labels.
            // (QML bindings on function calls are re-evaluated on signal.)
        }
    }
}
