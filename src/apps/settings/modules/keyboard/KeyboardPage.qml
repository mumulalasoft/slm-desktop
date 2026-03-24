import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property string highlightSettingId: ""
    property var layoutBinding: SettingsApp.createBinding("settings:keyboard/layout", "English (US)")
    property var repeatBinding: SettingsApp.createBindingFor("keyboard", "repeat", true)

    // ── Key-capture chip ─────────────────────────────────────────────────
    // Usage: ShortcutKey { prefKey: "windowing.bindClose"; defaultVal: "Alt+F4" }
    // Click to enter listening mode; press any non-modifier key combo to save;
    // Escape to cancel.
    component ShortcutKey: Item {
        id: sk

        property string prefKey: ""
        property string defaultVal: ""
        property bool listening: false
        property int _rev: 0

        readonly property string currentVal: {
            _rev // reactive trigger
            return UIPreferences.getPreference(prefKey, defaultVal)
        }

        implicitWidth: Math.max(chip.contentWidth + 24, 110)
        implicitHeight: 28

        // ── Helpers ──────────────────────────────────────────────────────
        function save(seq) {
            if (seq.length > 0)
                UIPreferences.setPreference(prefKey, seq)
            else
                UIPreferences.resetPreference(prefKey)
            _rev++
            listening = false
        }

        function keyEventToString(event) {
            // Ignore bare modifier presses
            if (event.key === Qt.Key_Control || event.key === Qt.Key_Shift ||
                event.key === Qt.Key_Alt     || event.key === Qt.Key_Meta  ||
                event.key === Qt.Key_Super_L || event.key === Qt.Key_Super_R)
                return ""

            var mods = []
            if (event.modifiers & Qt.MetaModifier)    mods.push("Meta")
            if (event.modifiers & Qt.ControlModifier) mods.push("Ctrl")
            if (event.modifiers & Qt.AltModifier)     mods.push("Alt")
            if (event.modifiers & Qt.ShiftModifier)   mods.push("Shift")

            var k = event.key
            var kStr = ""
            if (k >= Qt.Key_A && k <= Qt.Key_Z)
                kStr = String.fromCharCode(k)
            else if (k >= Qt.Key_0 && k <= Qt.Key_9)
                kStr = String.fromCharCode(k)
            else {
                const map = {
                    [Qt.Key_F1]:"F1",   [Qt.Key_F2]:"F2",   [Qt.Key_F3]:"F3",
                    [Qt.Key_F4]:"F4",   [Qt.Key_F5]:"F5",   [Qt.Key_F6]:"F6",
                    [Qt.Key_F7]:"F7",   [Qt.Key_F8]:"F8",   [Qt.Key_F9]:"F9",
                    [Qt.Key_F10]:"F10", [Qt.Key_F11]:"F11", [Qt.Key_F12]:"F12",
                    [Qt.Key_Space]:"Space",   [Qt.Key_Return]:"Return",
                    [Qt.Key_Tab]:"Tab",       [Qt.Key_Delete]:"Del",
                    [Qt.Key_Home]:"Home",     [Qt.Key_End]:"End",
                    [Qt.Key_PageUp]:"PgUp",   [Qt.Key_PageDown]:"PgDown",
                    [Qt.Key_Left]:"Left",     [Qt.Key_Right]:"Right",
                    [Qt.Key_Up]:"Up",         [Qt.Key_Down]:"Down",
                    [Qt.Key_Print]:"Print",   [Qt.Key_Pause]:"Pause",
                    [Qt.Key_Insert]:"Ins",    [Qt.Key_Minus]:"-",
                }
                kStr = map[k] || ""
            }
            if (!kStr) return ""
            return [...mods, kStr].join("+")
        }

        // ── Chip visuals ─────────────────────────────────────────────────
        Rectangle {
            id: chip
            anchors.verticalCenter: parent.verticalCenter
            width: parent.implicitWidth
            height: parent.height
            radius: 5
            color: sk.listening ? Theme.color("accent") : Theme.color("controlBg")
            border.width: 1
            border.color: sk.listening ? "transparent" : Theme.color("panelBorder")

            Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutQuad } }

            property real contentWidth: label.implicitWidth

            Text {
                id: label
                anchors.centerIn: parent
                text: sk.listening
                      ? qsTr("Press shortcut…")
                      : (sk.currentVal.length > 0 ? sk.currentVal : qsTr("(none)"))
                font.pixelSize: Theme.fontSize("small")
                font.family: (!sk.listening && sk.currentVal.length > 0)
                             ? "monospace" : Theme.fontFamilyUi
                color: sk.listening ? Theme.color("accentText") : Theme.color("textPrimary")
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { sk.listening = true; sk.forceActiveFocus() }
            }
        }

        // ── Key capture ───────────────────────────────────────────────────
        Keys.onPressed: function(event) {
            if (!sk.listening) return
            event.accepted = true
            if (event.key === Qt.Key_Escape) { sk.listening = false; return }
            const seq = sk.keyEventToString(event)
            if (seq) sk.save(seq)
        }

        onActiveFocusChanged: { if (!activeFocus) listening = false }

        Connections {
            target: UIPreferences
            function onPreferenceChanged() { sk._rev++ }
        }
    }
    // ── End ShortcutKey ───────────────────────────────────────────────────

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        // ── Keyboard ─────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Keyboard")
            Layout.fillWidth: true

            SettingCard {
                objectName: "layout"
                highlighted: root.highlightSettingId === "layout"
                label: qsTr("Input Source")
                description: qsTr("Choose keyboard language layout.")
                Layout.fillWidth: true
                ComboBox {
                    id: layoutCombo
                    model: ["English (US)", "Indonesian", "Japanese"]
                    currentIndex: Math.max(0, model.indexOf(String(root.layoutBinding.value)))
                    Layout.preferredWidth: 220
                    onActivated: root.layoutBinding.value = currentText
                }
            }

            SettingCard {
                objectName: "repeat"
                highlighted: root.highlightSettingId === "repeat"
                label: qsTr("Key Repeat")
                description: qsTr("Repeat keys while pressing.")
                Layout.fillWidth: true
                SettingToggle {
                    checked: Boolean(root.repeatBinding.value)
                    onToggled: root.repeatBinding.value = checked
                }
            }
        }

        // ── Shortcuts — Window ────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Window Shortcuts")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Close")
                description: qsTr("Close the active window.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindClose";     defaultVal: "Alt+F4"  }
            }
            SettingCard {
                label: qsTr("Minimize")
                description: qsTr("Minimize the active window.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindMinimize";  defaultVal: "Alt+F9"  }
            }
            SettingCard {
                label: qsTr("Maximize / Restore")
                description: qsTr("Maximize or restore the active window.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindMaximize";  defaultVal: "Alt+Up"  }
            }
            SettingCard {
                label: qsTr("Tile Left")
                description: qsTr("Snap the active window to the left half.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindTileLeft";  defaultVal: "Alt+Left"  }
            }
            SettingCard {
                label: qsTr("Tile Right")
                description: qsTr("Snap the active window to the right half.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindTileRight"; defaultVal: "Alt+Right" }
            }
            SettingCard {
                label: qsTr("Switch to Next")
                description: qsTr("Cycle focus to the next open window.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindSwitchNext"; defaultVal: "Alt+F11" }
            }
            SettingCard {
                label: qsTr("Switch to Previous")
                description: qsTr("Cycle focus to the previous open window.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindSwitchPrev"; defaultVal: "Alt+F12" }
            }
        }

        // ── Shortcuts — Workspace ─────────────────────────────────────────
        SettingGroup {
            title: qsTr("Workspace Shortcuts")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Workspace Panel")
                description: qsTr("Show the workspace overview panel.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "windowing.bindWorkspace";       defaultVal: "Alt+F3"          }
            }
            SettingCard {
                label: qsTr("Desktop Overview")
                description: qsTr("Toggle the desktop overview.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "shortcuts.workspaceOverview";   defaultVal: "Meta+S"          }
            }
            SettingCard {
                label: qsTr("Previous Workspace")
                description: qsTr("Switch to the previous workspace.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "shortcuts.workspacePrev";       defaultVal: "Meta+Left"       }
            }
            SettingCard {
                label: qsTr("Next Workspace")
                description: qsTr("Switch to the next workspace.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "shortcuts.workspaceNext";       defaultVal: "Meta+Right"      }
            }
            SettingCard {
                label: qsTr("Move Window Left")
                description: qsTr("Move the active window to the previous workspace.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "shortcuts.moveWindowPrev";      defaultVal: "Meta+Shift+Left" }
            }
            SettingCard {
                label: qsTr("Move Window Right")
                description: qsTr("Move the active window to the next workspace.")
                Layout.fillWidth: true
                ShortcutKey { prefKey: "shortcuts.moveWindowNext";      defaultVal: "Meta+Shift+Right"}
            }
        }
    }

    Connections {
        target: root.layoutBinding
        function onValueChanged() {
            layoutCombo.currentIndex = Math.max(0, layoutCombo.model.indexOf(String(root.layoutBinding.value)))
        }
    }
}
