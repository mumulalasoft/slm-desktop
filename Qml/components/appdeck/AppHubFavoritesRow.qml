import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as AppDeckComp

Item {
    id: root

    property var favoritesModel: []
    property int maxVisible: 8

    signal appActivated(var appData)

    implicitHeight: favoritesModel.length > 0 ? 136 : 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: "Quick Access"
            font.pixelSize: Theme.fontSize("body")
            font.weight: Theme.fontWeight("bold")
            color: Theme.color("textPrimary")
            opacity: Theme.opacityMuted
            visible: root.favoritesModel.length > 0
        }

        ListView {
            id: favoritesView
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            spacing: 10
            clip: true
            interactive: contentWidth > width
            model: root.favoritesModel.slice(0, Math.max(1, root.maxVisible))

            delegate: AppDeckComp.AppDeckItemDelegate {
                width: 96
                height: 104
                compact: true
                appData: modelData
                title: String((modelData && (modelData.display || modelData.name)) || "")
                iconSource: String((modelData && (modelData.icon || modelData.iconSource)) || "")
                running: !!(modelData && modelData.running)
                onActivated: function(appData) { root.appActivated(appData) }
            }
        }
    }
}
