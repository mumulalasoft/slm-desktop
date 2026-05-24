import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Row {
    id: root

    property string titleText: ""
    property string sortKey: "name"
    property bool showHidden: false
    property bool sortDescending: false
    property bool showTitle: true

    signal sortKeySelected(string key)
    signal toggleHiddenRequested()
    signal toggleSortDirectionRequested()

    width: parent ? parent.width : 0
    property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                ? ThemeIconController.revision : 0)

    height: Theme.metric("controlHeightLarge")
    spacing: Theme.metric("spacingSm")

    DSStyle.Label {
        visible: root.showTitle
        width: root.showTitle ? Math.max(0, parent.width - 238) : 0
        height: parent.height
        text: root.titleText
        color: Theme.color("textPrimary")
        font.pixelSize: Theme.fontSize("title")
        font.family: Theme.fontFamilyDisplay
        font.weight: Theme.fontWeight("semibold")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    DSStyle.ComboBox {
        id: topPortalSortCombo
        width: 126
        height: Theme.metric("controlHeightRegular")
        anchors.verticalCenter: parent.verticalCenter
        model: [
            { "label": "Name", "key": "name" },
            { "label": "Modified", "key": "date" },
            { "label": "Type", "key": "type" }
        ]
        textRole: "label"
        currentIndex: {
            if (root.sortKey === "date") return 1
            if (root.sortKey === "type") return 2
            return 0
        }
        onActivated: {
            var row = model[currentIndex] || ({ "key": "name" })
            root.sortKeySelected(String(row.key || "name"))
        }
    }

    Rectangle {
        id: hiddenButton
        width: 34
        height: Theme.metric("controlHeightRegular")
        anchors.verticalCenter: parent.verticalCenter
        radius: Theme.radiusControl
        color: hiddenMouse.pressed ? Theme.color("controlBgPressed")
                                   : (hiddenMouse.containsMouse || root.showHidden ? Theme.color("controlBgHover")
                                                                                  : Theme.color("controlBg"))
        border.width: Theme.borderWidthThin
        border.color: hiddenMouse.containsMouse ? Theme.color("panelBorderStrong") : Theme.color("panelBorder")

        Image {
            anchors.centerIn: parent
            width: 15
            height: 15
            fillMode: Image.PreserveAspectFit
            source: "image://themeicon/" + (root.showHidden ? "view-visible-symbolic" : "view-hidden-symbolic")
                    + "?v=" + root.iconRevision
            opacity: root.showHidden ? 1.0 : Theme.opacityMuted
        }

        MouseArea {
            id: hiddenMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.toggleHiddenRequested()
        }
        ToolTip.visible: hiddenMouse.containsMouse
        ToolTip.delay: 350
        ToolTip.text: root.showHidden ? qsTr("Hide hidden files") : qsTr("Show hidden files")
    }

    Rectangle {
        id: sortDirectionButton
        width: 34
        height: Theme.metric("controlHeightRegular")
        anchors.verticalCenter: parent.verticalCenter
        radius: Theme.radiusControl
        color: sortMouse.pressed ? Theme.color("controlBgPressed")
                                 : (sortMouse.containsMouse ? Theme.color("controlBgHover") : Theme.color("controlBg"))
        border.width: Theme.borderWidthThin
        border.color: sortMouse.containsMouse ? Theme.color("panelBorderStrong") : Theme.color("panelBorder")

        Image {
            anchors.centerIn: parent
            width: 15
            height: 15
            fillMode: Image.PreserveAspectFit
            source: "image://themeicon/" + (root.sortDescending ? "view-sort-descending-symbolic" : "view-sort-ascending-symbolic")
                    + "?v=" + root.iconRevision
        }

        MouseArea {
            id: sortMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.toggleSortDirectionRequested()
        }
        ToolTip.visible: sortMouse.containsMouse
        ToolTip.delay: 350
        ToolTip.text: root.sortDescending ? qsTr("Sort descending") : qsTr("Sort ascending")
    }
}
