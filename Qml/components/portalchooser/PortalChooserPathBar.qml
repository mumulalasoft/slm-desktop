import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root

    property bool pathEditMode: false
    property string currentDir: ""
    property int iconRevision: 0
    property var breadcrumbSegments: []

    readonly property bool editorActiveFocus: portalDirField.activeFocus
    readonly property string editorText: portalDirField.text

    signal pathEditModeChangeRequested(bool value)
    signal loadDirectoryRequested(string path)

    width: parent ? parent.width : 0
    height: Theme.metric("controlHeightLarge")

    function focusEditorAndSelectAll() {
        pathEditModeChangeRequested(true)
        portalDirField.forceActiveFocus()
        portalDirField.selectAll()
    }

    function clearEditorFocus() {
        portalDirField.focus = false
    }

    Row {
        anchors.fill: parent
        spacing: Theme.metric("spacingXs")

        Item {
            id: pathStackArea
            width: Math.max(0, parent.width - 42)
            height: parent.height

            DSStyle.TextField {
                id: portalDirField
                anchors.fill: parent
                opacity: root.pathEditMode ? 1 : 0
                visible: opacity > 0.01
                enabled: root.pathEditMode
                text: root.currentDir
                color: Theme.color("textPrimary")
                selectionColor: Theme.color("selectedItem")
                selectedTextColor: Theme.color("selectedItemText")
                placeholderText: "Directory"
                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingStandard }
                }
                onAccepted: {
                    root.loadDirectoryRequested(text)
                    root.pathEditModeChangeRequested(false)
                }
            }

            Flickable {
                id: portalPathFlick
                anchors.fill: parent
                opacity: root.pathEditMode ? 0 : 1
                visible: opacity > 0.01
                enabled: !root.pathEditMode
                contentWidth: portalPathBreadcrumbRow.width
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick
                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingStandard }
                }
                onContentWidthChanged: Qt.callLater(scrollToEnd)
                onWidthChanged: Qt.callLater(scrollToEnd)

                function scrollToEnd() {
                    contentX = Math.max(0, contentWidth - width)
                }

                Row {
                    id: portalPathBreadcrumbRow
                    height: parent.height
                    spacing: 0
                    Repeater {
                        model: root.breadcrumbSegments
                        delegate: Row {
                            required property var modelData
                            required property int index
                            height: parent.height
                            spacing: 0
                            width: (index > 0 ? crumbSeparator.width : 0) + crumbButton.width

                            DSStyle.Label {
                                id: crumbSeparator
                                visible: index > 0
                                width: visible ? 16 : 0
                                height: parent.height
                                text: "/"
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Rectangle {
                                id: crumbButton
                                property string text: String(modelData.label || "")
                                readonly property bool rootCrumb: text === "/"
                                readonly property bool lastCrumb: index === root.breadcrumbSegments.length - 1
                                height: Theme.metric("controlHeightRegular")
                                width: {
                                    var desired = crumbLabel.implicitWidth + Theme.metric("spacingLg")
                                    if (rootCrumb) {
                                        return 34
                                    }
                                    return Math.max(lastCrumb ? 72 : 58, Math.min(desired, lastCrumb ? 220 : 160))
                                }
                                anchors.verticalCenter: parent.verticalCenter
                                radius: Theme.radiusMdPlus
                                color: crumbMouse.pressed ? Theme.color("controlBgPressed")
                                                          : (crumbMouse.containsMouse || lastCrumb ? Theme.color("controlBgHover") : "transparent")
                                border.width: lastCrumb || crumbMouse.containsMouse ? Theme.borderWidthThin : Theme.borderWidthNone
                                border.color: crumbMouse.containsMouse ? Theme.color("panelBorderStrong") : Theme.color("panelBorder")

                                Image {
                                    visible: crumbButton.rootCrumb
                                    anchors.centerIn: parent
                                    width: 15
                                    height: 15
                                    fillMode: Image.PreserveAspectFit
                                    source: "image://themeicon/go-home?v=" + root.iconRevision
                                    opacity: Theme.opacityIconMuted
                                }

                                DSStyle.Label {
                                    id: crumbLabel
                                    visible: !crumbButton.rootCrumb
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.metric("spacingSm")
                                    anchors.rightMargin: Theme.metric("spacingSm")
                                    text: crumbButton.text
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    id: crumbMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.loadDirectoryRequested(String(modelData.path || "~"))
                                }
                                ToolTip.visible: crumbMouse.containsMouse && crumbLabel.truncated
                                ToolTip.delay: 350
                                ToolTip.text: String(modelData.path || crumbButton.text)
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    propagateComposedEvents: true
                    onPressed: mouse.accepted = false
                    onClicked: mouse.accepted = false
                    onDoubleClicked: root.focusEditorAndSelectAll()
                }
            }
        }

        Rectangle {
            id: portalPathModeButton
            width: 36
            height: Theme.metric("controlHeightRegular")
            anchors.verticalCenter: parent.verticalCenter
            property bool editWasActiveOnPress: false
            radius: Theme.radiusControl
            color: pathModeMouse.pressed ? Theme.color("controlBgPressed")
                                         : (pathModeMouse.containsMouse || root.pathEditMode ? Theme.color("controlBgHover")
                                                                                            : Theme.color("controlBg"))
            border.width: Theme.borderWidthThin
            border.color: pathModeMouse.containsMouse ? Theme.color("panelBorderStrong") : Theme.color("panelBorder")

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                fillMode: Image.PreserveAspectFit
                source: "image://themeicon/"
                        + (root.pathEditMode ? "object-select-symbolic" : "document-edit-symbolic")
                        + "?v=" + root.iconRevision
            }

            MouseArea {
                id: pathModeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onPressed: portalPathModeButton.editWasActiveOnPress = root.pathEditMode
                onClicked: {
                    if (portalPathModeButton.editWasActiveOnPress) {
                        root.pathEditModeChangeRequested(false)
                        root.forceActiveFocus()
                        return
                    }
                    root.focusEditorAndSelectAll()
                }
            }
            ToolTip.visible: pathModeMouse.containsMouse
            ToolTip.delay: 350
            ToolTip.text: root.pathEditMode ? qsTr("Use breadcrumbs") : qsTr("Edit location")
        }
    }

    MouseArea {
        id: autoCloseArea
        x: pathStackArea.x
        y: pathStackArea.y
        width: pathStackArea.width
        height: pathStackArea.height
        z: 100
        visible: root.pathEditMode
        enabled: visible
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true
        onPressed: function(mouse) {
            var local = portalDirField.mapFromItem(autoCloseArea, mouse.x, mouse.y)
            var insideEditor = local.x >= 0 && local.y >= 0
                    && local.x < portalDirField.width && local.y < portalDirField.height
            if (!insideEditor) {
                root.pathEditModeChangeRequested(false)
            }
            mouse.accepted = false
        }
    }
}
