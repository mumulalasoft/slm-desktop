import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    // Property surface preserved so callers don't break, even though title
    // and counter are no longer rendered.
    property string title: "AppDeck"
    property string searchText: ""
    property int totalCount: 0
    property int filteredCount: 0

    // docs/APPDECK.md §17 — Grid→Pulse unified context. Header exposes its
    // focus/typing state so the parent can drive the State 1/2/3 backdrop:
    //   idle    (no focus, no text): full grid
    //   focused (focus, no text):    dim+scale grid (State 2 ambience)
    //   active  (text present):      Pulse mode (State 3)
    readonly property bool searchFocused: searchField.activeFocus
    readonly property bool searchActive: String(searchText || "").length > 0

    signal queryChanged(string text)
    signal collapseRequested()
    signal focusGridRequested()
    // §9 — Keyboard forwarding for pulse selection. Pulse no longer has its
    // own header field, so Up/Down/Enter/Tab pressed while the grid search
    // is focused get forwarded upward for the parent to route into pulse
    // selection actions (move selection, activate result, switch section).
    signal pulseNavigateRequested(int delta)
    signal pulseNavigateSectionRequested(int delta)
    signal pulseActivateRequested()

    // Spotlight-style glass pill search — a single, centered, translucent
    // capsule. No title bar, no close button. The pill is the only chrome
    // in the header so it carries enough visual weight to sit alone.
    implicitHeight: 56

    Item {
        anchors.centerIn: parent
        width: Math.min(560, Math.max(260, parent.width - 32))
        height: 40

        // Glass pill background. Translucent fill + subtle hairline border
        // gives a soft "frosted" feel that reads on both light and dark
        // backgrounds without competing with the icons below. §9 APPDECK.md
        // requires the pill to read as a spatial anchor in pulse mode — the
        // active tier amps fill/border so the field stays the visual origin
        // even as the grid backdrop dims behind it.
        Rectangle {
            id: pill
            anchors.fill: parent
            radius: height / 2
            color: root.searchActive
                   ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.16 : 0.66)
                   : (searchField.activeFocus
                      ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.12 : 0.55)
                      : Qt.rgba(1, 1, 1, Theme.darkMode ? 0.07 : 0.40))
            border.width: Theme.borderWidthThin
            border.color: root.searchActive
                          ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.32 : 0.78)
                          : (searchField.activeFocus
                             ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.22 : 0.65)
                             : Qt.rgba(1, 1, 1, Theme.darkMode ? 0.10 : 0.45))
            antialiasing: true

            Behavior on color {
                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
            }
            Behavior on border.color {
                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
            }
        }

        // Magnifier glyph drawn as a circle + tail. Lives inside the pill
        // at a fixed left inset.
        Item {
            id: glyph
            width: 16
            height: 16
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            opacity: root.searchActive
                     ? 1.0
                     : (searchField.activeFocus ? 0.85 : 0.55)

            Behavior on opacity {
                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
            }

            Rectangle {
                id: glyphCircle
                anchors.left: parent.left
                anchors.top: parent.top
                width: 11
                height: 11
                radius: width / 2
                color: "transparent"
                border.width: Theme.borderWidthThin
                border.color: Theme.color("textPrimary")
                antialiasing: true
            }
            Rectangle {
                x: 10
                y: 11
                width: 6
                height: 1.6
                radius: 0.8
                rotation: 45
                color: Theme.color("textPrimary")
                antialiasing: true
            }
        }

        TextField {
            id: searchField
            anchors.fill: parent
            anchors.leftMargin: 38
            anchors.rightMargin: clearButton.visible ? 38 : 16
            // docs/APPDECK.md §17 — placeholder shifts per state so the user
            // reads "browse" → "intent" → "search" as they engage the field.
            placeholderText: root.searchActive
                             ? qsTr("Search apps, files, actions…")
                             : (root.searchFocused
                                ? qsTr("Type a command or search")
                                : qsTr("Search apps"))
            placeholderTextColor: Qt.rgba(
                Theme.color("textSecondary").r,
                Theme.color("textSecondary").g,
                Theme.color("textSecondary").b,
                0.55)
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("body")
            text: root.searchText
            background: null
            selectByMouse: true
            verticalAlignment: TextInput.AlignVCenter

            onTextChanged: {
                if (root.searchText === text) {
                    return
                }
                root.searchText = text
                root.queryChanged(text)
            }

            Keys.onPressed: function(event) {
                // §9 — When pulse is active (text typed), keyboard nav is
                // routed to the pulse selection. When idle/empty, Down
                // moves focus to the grid below as before.
                if (event.key === Qt.Key_Down) {
                    if (root.searchActive) {
                        root.pulseNavigateRequested(1)
                    } else {
                        root.focusGridRequested()
                    }
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Up) {
                    if (root.searchActive) {
                        root.pulseNavigateRequested(-1)
                        event.accepted = true
                        return
                    }
                }
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    if (root.searchActive) {
                        root.pulseActivateRequested()
                        event.accepted = true
                        return
                    }
                }
                if (event.key === Qt.Key_Tab) {
                    if (root.searchActive) {
                        root.pulseNavigateSectionRequested(1)
                        event.accepted = true
                        return
                    }
                }
                if (event.key === Qt.Key_Backtab) {
                    if (root.searchActive) {
                        root.pulseNavigateSectionRequested(-1)
                        event.accepted = true
                        return
                    }
                }
                if (event.key === Qt.Key_Escape) {
                    if (root.searchText.length > 0) {
                        root.searchText = ""
                        root.queryChanged("")
                        event.accepted = true
                        return
                    }
                    root.collapseRequested()
                    event.accepted = true
                }
            }
        }

        // Clear button — a small circular "x" that appears only when the
        // field has content. HIG-aligned: clear affordance lives inside
        // the field, not as a separate header button.
        Item {
            id: clearButton
            width: 22
            height: 22
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            visible: root.searchText.length > 0
            opacity: clearHover.containsMouse ? 1.0 : 0.72

            Behavior on opacity {
                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
            }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.35 : 0.18)
                antialiasing: true
            }

            // X glyph drawn as two diagonal bars.
            Rectangle {
                width: 10
                height: 1.5
                radius: 0.8
                color: Theme.color("textPrimary")
                anchors.centerIn: parent
                rotation: 45
                antialiasing: true
            }
            Rectangle {
                width: 10
                height: 1.5
                radius: 0.8
                color: Theme.color("textPrimary")
                anchors.centerIn: parent
                rotation: -45
                antialiasing: true
            }

            MouseArea {
                id: clearHover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.searchText = ""
                    root.queryChanged("")
                    searchField.forceActiveFocus()
                }
            }
        }
    }
}
