import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Rectangle {
    id: root

    property var placesModel: null
    property string currentDir: ""
    property int iconRevision: 0

    signal directoryRequested(string path)
    signal mountRequested(string device)
    signal unmountRequested(string device)

    width: 206
    height: parent ? parent.height : 0
    radius: Theme.radiusControlLarge
    color: Theme.color("fileManagerWindowBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("fileManagerWindowBorder")

    Item {
        anchors.fill: parent
        anchors.margins: 6

        Flickable {
            id: placesFlick
            anchors.fill: parent
            clip: true
            contentWidth: width
            contentHeight: placesColumn.height

            Column {
                id: placesColumn
                width: placesFlick.width
                spacing: 1

                Repeater {
                    model: root.placesModel

                    delegate: Item {
                        required property bool header
                        required property string label
                        required property string path
                        required property string icon
                        required property bool mounted
                        required property string device
                        required property bool storage
                        required property bool isRoot

                        width: placesColumn.width
                        height: header ? 20 : 29
                        clip: true

                        Text {
                            visible: header
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: label
                            color: Theme.color("textSecondary")
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("xs")
                            font.weight: Theme.fontWeight("bold")
                            elide: Text.ElideRight
                            wrapMode: Text.NoWrap
                            verticalAlignment: Text.AlignVCenter
                        }

                        Rectangle {
                            id: placeRowBg
                            visible: !header
                            anchors.fill: parent
                            radius: Theme.radiusMdPlus
                            color: String(root.currentDir || "") === path
                                   ? Theme.color("accentSoft")
                                   : (placeMouse.containsMouse ? Theme.color("accentSoft") : "transparent")
                        }

                        Item {
                            visible: !header
                            anchors.fill: parent

                            Image {
                                width: 16
                                height: 16
                                fillMode: Image.PreserveAspectFit
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                source: "image://themeicon/" + String(icon || "folder") + "?v=" + root.iconRevision
                                opacity: (!!storage && !mounted) ? 0.55 : 1.0
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 32
                                anchors.right: parent.right
                                anchors.rightMargin: ((storage && !isRoot) ? 34 : 8)
                                anchors.verticalCenter: parent.verticalCenter
                                text: label
                                color: (!!storage && !mounted)
                                       ? Theme.color("textSecondary")
                                       : Theme.color("textPrimary")
                                font.family: Theme.fontFamilyUi
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.NoWrap
                            }

                            Rectangle {
                                id: storageMountButton
                                visible: storage && !isRoot
                                width: 20
                                height: 20
                                radius: Theme.radiusMd
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                color: storageMountMouse.containsMouse
                                       ? Theme.color("accentSoft")
                                       : "transparent"
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color("menuBorder")

                                Image {
                                    anchors.centerIn: parent
                                    width: 12
                                    height: 12
                                    fillMode: Image.PreserveAspectFit
                                    source: "image://themeicon/"
                                            + (mounted ? "media-eject-symbolic" : "media-mount-symbolic")
                                            + "?v=" + root.iconRevision
                                    opacity: Theme.opacityIconMuted
                                }

                                MouseArea {
                                    id: storageMountMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: function(mouse) {
                                        mouse.accepted = true
                                        var dev = String(device || "")
                                        if (dev.length <= 0) {
                                            return
                                        }
                                        if (mounted) {
                                            root.unmountRequested(dev)
                                        } else {
                                            root.mountRequested(dev)
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: placeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            enabled: !header
                            onClicked: {
                                var p = String(path || "")
                                if (p.length > 0) {
                                    root.directoryRequested(p)
                                    return
                                }
                                if (!!storage && String(device || "").length > 0) {
                                    root.mountRequested(String(device || ""))
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
