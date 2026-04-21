import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property string displayName: ""
    property string iconName: ""
    property string iconSource: ""
    property string thumbnailSource: ""
    property bool selected: false
    property bool hovered: false
    property bool dragging: false
    property bool isDir: false
    property bool networkShared: false
    property int tileWidth: 96
    property int tileHeight: 108
    property bool previewCandidate: false
    property bool editing: false
    property string editText: ""

    signal pressed(real x, real y, int button, int buttons, int modifiers)
    signal moved(real x, real y, int buttons, int modifiers)
    signal released(real x, real y, int button, int buttons, int modifiers)
    signal clicked(int button, int modifiers, real x, real y)
    signal doubleClicked(int button, int modifiers, real x, real y)
    signal editValueChanged(string text)
    signal editCommitted(string text)
    signal editCanceled()

    width: tileWidth
    height: tileHeight

    Rectangle {
        id: selectionBg
        anchors.fill: parent
        radius: Theme.radiusCard
        color: root.selected ? Theme.color("accentSoft") : "transparent"
        border.width: root.selected ? Theme.borderWidthThin : Theme.borderWidthNone
        border.color: Theme.color("dragGhostBorder")
        opacity: root.dragging ? 0.72 : 1.0
        visible: !root.editing
    }

    Item {
        id: iconStage
        width: parent.width
        height: Math.max(48, parent.height - 38)
        anchors.top: parent.top

        Image {
            id: thumbImage
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height)
            height: width
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            source: (root.previewCandidate && String(root.thumbnailSource || "").length > 0)
                    ? root.thumbnailSource : ""
            visible: source.length > 0 && status === Image.Ready
        }

        Image {
            id: iconImage
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height)
            height: width
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            source: {
                if (thumbImage.visible) {
                    return ""
                }
                if (String(root.iconSource || "").length > 0) {
                    return root.iconSource
                }
                if (String(root.iconName || "").length > 0) {
                    var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                               ? ThemeIconController.revision : 0)
                    return "image://themeicon/" + root.iconName + "?v=" + rev
                }
                var fallbackRev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                   ? ThemeIconController.revision : 0)
                return "image://themeicon/text-x-generic-symbolic?v=" + fallbackRev
            }
        }

        Image {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 16
            height: 16
            visible: root.networkShared
            fillMode: Image.PreserveAspectFit
            source: "qrc:/icons/emblems/emblem-shared-symbolic.svg"
        }
    }

    Label {
        id: nameText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !root.editing
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignTop
        text: root.displayName
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
        lineHeightMode: Text.ProportionalHeight
        lineHeight: Theme.lineHeight("tight")
        color: root.selected ? Theme.color("accentText") : Theme.color("selectedItemText")
        font.pixelSize: Theme.fontSize("small")
    }

    TextField {
        id: renameField
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.editing
        text: root.editText
        selectByMouse: true
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Theme.fontSize("small")
        onTextChanged: root.editValueChanged(text)
        onEditingFinished: {
            if (root.editing) {
                root.editCommitted(String(text || "").trim())
            }
        }
        Keys.onEscapePressed: function(event) {
            event.accepted = true
            root.editCanceled()
        }
    }

    onEditingChanged: {
        if (!editing) {
            return
        }
        Qt.callLater(function() {
            if (!renameField || !root.editing) {
                return
            }
            renameField.forceActiveFocus()
            var current = String(renameField.text || "")
            if (root.isDir) {
                renameField.selectAll()
                return
            }
            var dot = current.lastIndexOf(".")
            if (dot > 0) {
                renameField.select(0, dot)
            } else {
                renameField.selectAll()
            }
        })
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        enabled: !root.editing
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onPressed: function(mouse) {
            root.pressed(mouse.x, mouse.y, mouse.button, mouse.buttons, mouse.modifiers)
        }
        onPositionChanged: function(mouse) {
            root.moved(mouse.x, mouse.y, mouse.buttons, mouse.modifiers)
        }
        onReleased: function(mouse) {
            root.released(mouse.x, mouse.y, mouse.button, mouse.buttons, mouse.modifiers)
        }
        onClicked: function(mouse) {
            root.clicked(mouse.button, mouse.modifiers, mouse.x, mouse.y)
        }
        onDoubleClicked: function(mouse) {
            root.doubleClicked(mouse.button, mouse.modifiers, mouse.x, mouse.y)
        }
    }

}
