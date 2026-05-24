import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Effects
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
    property bool interactable: true
    property real _shadowLift: dragging ? 1.0 : ((hovered || selected) ? 0.45 : 0.0)

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
    scale: root.dragging ? 1.06 : 1.0

    function _themeIconSource(name) {
        var n = String(name || "").trim()
        var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                   ? ThemeIconController.revision : 0)
        if (n.length <= 0) {
            n = "text-x-generic-symbolic"
        }
        if (n.indexOf("image://themeicon/") === 0) {
            return n + (n.indexOf("?") >= 0 ? "&v=" : "?v=") + rev
        }
        return "image://themeicon/" + n + "?v=" + rev
    }

    function _isLocalFilePath(value) {
        var text = String(value || "").trim()
        return text.length > 0 && text.charAt(0) === "/"
    }

    function _providerLocalSource(value) {
        var raw = String(value || "").trim()
        if (raw.length <= 0) {
            return ""
        }
        if (raw.indexOf("image://themeicon/") === 0) {
            return raw
        }
        if (raw.indexOf("file://") === 0) {
            raw = raw.slice("file://".length)
        }
        if (_isLocalFilePath(raw)) {
            return "image://themeicon/" + encodeURIComponent(raw) + "?v="
                   + ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                      ? ThemeIconController.revision : 0)
        }
        return raw
    }

    function _normalizedImageSource(value) {
        var raw = String(value || "").trim()
        if (raw.length <= 0) {
            return ""
        }
        if (raw.indexOf("image://themeicon/") === 0) {
            return raw
        }
        if (raw.indexOf("file://") === 0) {
            return _providerLocalSource(raw)
        }
        if (raw.indexOf("image://") === 0 || raw.indexOf("qrc:/") === 0) {
            return raw
        }
        if (_isLocalFilePath(raw)) {
            return _providerLocalSource(raw)
        }
        return raw
    }

    function _preferredIconSource() {
        var name = String(root.iconName || "").trim()
        var source = String(root.iconSource || "").trim()
        if (source.length > 0) {
            return _normalizedImageSource(source)
        }
        if (name.length <= 0) {
            return ""
        }
        if (_isLocalFilePath(name)) {
            return _providerLocalSource(name)
        }
        if (name.indexOf("://") >= 0) {
            return _normalizedImageSource(name)
        }
        return root._themeIconSource(name)
    }

    function _secondaryIconSource() {
        var name = String(root.iconName || "").trim()
        var source = String(root.iconSource || "").trim()
        if (name.length <= 0 && source.length <= 0) {
            return ""
        }
        if (source.length > 0) {
            return _normalizedImageSource(source)
        }
        if (_isLocalFilePath(name)) {
            return _providerLocalSource(name)
        }
        if (name.indexOf("://") >= 0) {
            return _normalizedImageSource(name)
        }
        return root._themeIconSource(name)
    }

    function _genericIconSource() {
        return root._themeIconSource("application-x-executable-symbolic")
    }

    Rectangle {
        id: selectionBg
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        radius: Theme.radiusCard
        color: root.selected
               ? Theme.color("accentSubtle")
               : (root.hovered ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.10 : 0.18) : "transparent")
        border.width: root.selected ? Theme.borderWidthThin : Theme.borderWidthNone
        border.color: Theme.color("panelBorderStrong")
        opacity: root.dragging ? Theme.opacityMuted : 1.0
        visible: !root.editing && (root.selected || root.hovered || root.dragging)

        Behavior on color {
            ColorAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingDefault
            }
        }
        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingDefault
            }
        }
    }

    Item {
        id: iconStage
        width: parent.width
        height: Math.max(48, parent.height - 38)
        anchors.top: parent.top
        // Keep the icon stage unlayered. We only need the icon itself here;
        // the extra layer + MultiEffect path has proven fragile on some
        // render backends and can leave the icon slot blank even when the
        // source resolves correctly.
        layer.enabled: false

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
            visible: String(source || "").length > 0 && status === Image.Ready
        }

        Image {
            id: iconImage
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height)
            height: width
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            source: root._preferredIconSource()
            visible: String(source || "").length > 0
                     && status !== Image.Error
            onSourceChanged: {
                console.log("[desktop-item-icon] iconImage source="
                            + String(source || "")
                            + " iconName=" + String(root.iconName || "")
                            + " iconSource=" + String(root.iconSource || "")
                            + " displayName=" + String(root.displayName || ""))
            }
            onStatusChanged: {
                console.log("[desktop-item-icon] iconImage status="
                            + String(status)
                            + " source=" + String(source || "")
                            + " visible=" + String(visible))
            }
        }

        Image {
            id: fallbackIconImage
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height)
            height: width
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            source: root._secondaryIconSource()
            visible: String(source || "").length > 0
                     && iconImage.status === Image.Error
                     && status !== Image.Error
            onStatusChanged: {
                console.log("[desktop-item-icon] fallback status="
                            + String(status)
                            + " source=" + String(source || "")
                            + " visible=" + String(visible))
            }
        }

        Image {
            id: genericFallbackIconImage
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height)
            height: width
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            source: root._genericIconSource()
            visible: iconImage.status === Image.Error
                     && (String(fallbackIconImage.source || "").length <= 0 || fallbackIconImage.status === Image.Error)
                     && status !== Image.Error
            onStatusChanged: {
                console.log("[desktop-item-icon] generic status="
                            + String(status)
                            + " source=" + String(source || "")
                            + " visible=" + String(visible))
            }
        }

        Image {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 16
            height: 16
            visible: root.networkShared
            fillMode: Image.PreserveAspectFit
            source: {
                var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                           ? ThemeIconController.revision : 0)
                return "image://themeicon/emblem-shared-symbolic?v=" + rev
            }
        }
    }

    Label {
        id: nameText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: Math.min(parent.width - Theme.metric("spacingSm"), implicitWidth + Theme.metric("spacingSm"))
        height: Math.min(implicitHeight + Theme.metric("spacingXxs"), parent.height * 0.42)
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
        padding: Theme.metric("spacingXxs")
        background: Rectangle {
            radius: Theme.radiusMd
            color: root.selected
                   ? Theme.color("accent")
                   : Qt.rgba(0, 0, 0, Theme.darkMode ? 0.40 : 0.32)
        }
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
        enabled: !root.editing && root.interactable
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onCanceled: root.hovered = false
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

    Behavior on _shadowLift {
        NumberAnimation {
            duration: Theme.durationFast
            easing.type: Theme.easingDefault
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: Theme.durationNormal
            easing.type: Theme.easingSpring
            easing.overshoot: 0.4
        }
    }

}
