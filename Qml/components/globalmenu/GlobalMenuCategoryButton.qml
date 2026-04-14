import QtQuick
import QtQuick.Controls
import Slm_Desktop

// GlobalMenuCategoryButton — a single top-level menu label in the global menu bar.
//
// Design: text label with a sliding underline on hover/active.
// NOT a background-pill highlight — the underline is the identifier.

Item {
    id: root

    property string label: ""
    property bool menuEnabled: true
    property bool active: false   // true while its dropdown is open

    signal clicked()

    implicitWidth: label.implicitWidth + Theme.metric("spacingMd") * 2
    implicitHeight: parent ? parent.height : 28

    // ── label ────────────────────────────────────────────────────────────────
    Text {
        id: label
        anchors.centerIn: parent
        text: root.label
        color: root.menuEnabled
               ? (hoverHandler.hovered || root.active
                  ? Theme.color("textPrimary")
                  : Theme.color("textOnGlass"))
               : Theme.color("textDisabled")
        font.pixelSize: Theme.fontSize("bodyLarge")
        font.weight: root.active ? Theme.fontWeight("semibold") : Theme.fontWeight("regular")

        Behavior on color {
            ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }
        Behavior on font.weight {
            enabled: false  // weight changes can't animate; disable to avoid glitch
        }
    }

    // ── underline ────────────────────────────────────────────────────────────
    Rectangle {
        id: underline
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter

        height: root.active ? 2 : 1
        width: root.active
               ? label.width + Theme.metric("spacingSm")
               : (hoverHandler.hovered ? label.width : 0)

        radius: Theme.radiusHairline
        color: root.active
               ? Theme.color("accent")
               : Theme.color("textOnGlass")
        opacity: root.active ? 1.0 : (hoverHandler.hovered ? 0.55 : 0)

        Behavior on width {
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
        }
        Behavior on height {
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }
        Behavior on color {
            ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }
    }

    // ── interaction ──────────────────────────────────────────────────────────
    HoverHandler {
        id: hoverHandler
        cursorShape: root.menuEnabled ? Qt.ArrowCursor : Qt.ForbiddenCursor
    }

    TapHandler {
        enabled: root.menuEnabled
        onTapped: root.clicked()
    }
}
