import QtQuick
import QtQuick.Controls
import Slm_Desktop
import SlmStyle as DSStyle

// DesktopContextMenu — right-click menu for the desktop surface.
//
// Design decisions (SLM Senior Designer, 2026-05-20):
//
// 1. Migrated from QtQuick.Controls Menu (unstyled) to DSStyle.Menu — gives us
//    the styled surface (blur, border, radius, shadow) and MenuItem components
//    (correct typography, hover animation, shortcutText slot) for free.
//
// 2. Entrance animation: fade-in (opacity 0→1) + slight upward translate
//    (y +6px → 0) over 160 ms with easingDecelerate. This matches the feel of
//    macOS Sonoma context menus without being showy. The same enter/exit
//    transition is applied symmetrically (exit is faster at 110 ms).
//
// 3. Structure follows macOS Finder desktop menu convention:
//      [creation]  New Folder / New Text Document
//      ─────────
//      [clipboard] Paste
//      ─────────
//      [layout]    Sort By ▶  /  Clean Up Desktop
//      ─────────
//      [system]    Change Desktop Wallpaper...  /  Show View Options...
//      ─────────
//      [utility]   New Terminal at Desktop
//
//    Removed: "Settings" — not a desktop context menu convention; it violates
//    the principle of direct manipulation (settings are for the app, not the
//    surface). "Appearance..." kept as "Show View Options..." — clearer scope.
//
// 4. Labels rewritten to noun-phrase convention used by macOS:
//    "Arrange Icons By" → "Sort By"
//    "Open Terminal Here" → "New Terminal at Desktop"
//    "Change Wallpaper..." → "Change Desktop Wallpaper..."
//    "Appearance..." → "Show View Options..."
//
// 5. Sort By promoted to a top-level submenu (not nested in Arrange). Clean Up
//    Desktop moved to top level — it is a distinct action, not a sort variant.
//
// 6. Keyboard shortcuts shown for Paste (Ctrl+V) — the only standard desktop
//    shortcut in this menu. Others omitted to avoid visual clutter on actions
//    that have no universal desktop-level shortcut.
//
// 7. Icons: intentionally omitted. macOS Sequoia/Sonoma desktop context menus
//    do NOT use icons on creation/layout items. Icons would add visual weight
//    without adding clarity here. DSStyle.MenuItem icon slots remain available
//    if the project later adopts a system-icon set.
//
// Signal interface is preserved 1:1 — no breaking changes for DesktopSurface.

Item {
    id: root

    property string desktopPath: ""
    property bool handleRightClick: false
    property alias menuVisible: desktopMenu.visible
    property double _lastPopupAtMs: 0
    property real _lastPopupX: -99999
    property real _lastPopupY: -99999

    signal newFolder()
    signal newTextFile()
    signal paste()
    signal arrangeIcons()
    signal snapToGrid()
    signal cleanUpDesktop()
    signal arrangeIconsBy(string sortKey, bool descending)
    signal changeWallpaper()
    signal openAppearance()
    signal openSettings()
    signal openTerminal()

    function openFolderMenu(x, y) {
        var nx = Number(x || 0)
        var ny = Number(y || 0)
        var nowMs = Date.now()
        if ((nowMs - Number(_lastPopupAtMs || 0)) < 140
                && Math.abs(nx - Number(_lastPopupX || 0)) <= 2
                && Math.abs(ny - Number(_lastPopupY || 0)) <= 2) {
            return
        }
        _lastPopupAtMs = nowMs
        _lastPopupX = nx
        _lastPopupY = ny
        desktopMenu.popup(nx, ny)
    }

    function closeMenu() {
        if (desktopMenu.visible) {
            desktopMenu.close()
        }
    }

    TapHandler {
        enabled: root.handleRightClick
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint) {
            root.openFolderMenu(eventPoint.position.x, eventPoint.position.y)
        }
    }

    DSStyle.Menu {
        id: desktopMenu
        parent: Overlay.overlay
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent

        // ── Entrance / exit animation ─────────────────────────────────────────
        // Fade + upward slide mimics macOS context menu appear behaviour.
        // Duration and easing use Theme tokens so safe mode / reduced motion
        // are automatically respected.
        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: Theme.animationsEnabled ? 160 : 1
                    easing.type: Theme.easingDecelerate
                }
                NumberAnimation {
                    property: "y"
                    from: desktopMenu.y + 6
                    to: desktopMenu.y
                    duration: Theme.animationsEnabled ? 160 : 1
                    easing.type: Theme.easingDecelerate
                }
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: Theme.animationsEnabled ? 110 : 1
                easing.type: Theme.easingAccelerate
            }
        }

        // ── Group 1: Creation ─────────────────────────────────────────────────
        DSStyle.MenuItem {
            text: qsTr("New Folder")
            onTriggered: root.newFolder()
        }
        DSStyle.MenuItem {
            text: qsTr("New Text Document")
            onTriggered: root.newTextFile()
        }

        DSStyle.MenuSeparator {}

        // ── Group 2: Clipboard ────────────────────────────────────────────────
        DSStyle.MenuItem {
            text: qsTr("Paste")
            shortcutText: "Ctrl+V"
            enabled: (typeof ClipboardServiceClient !== "undefined"
                      && ClipboardServiceClient
                      && ClipboardServiceClient.hasContent === true)
            onTriggered: root.paste()
        }

        DSStyle.MenuSeparator {}

        // ── Group 3: Layout ───────────────────────────────────────────────────
        DSStyle.Menu {
            id: sortByMenu
            title: qsTr("Sort By")

            DSStyle.MenuItem {
                text: qsTr("Name")
                onTriggered: root.arrangeIconsBy("name", false)
            }
            DSStyle.MenuItem {
                text: qsTr("Date Modified")
                onTriggered: root.arrangeIconsBy("dateModified", true)
            }
            DSStyle.MenuItem {
                text: qsTr("Size")
                onTriggered: root.arrangeIconsBy("size", true)
            }
            DSStyle.MenuItem {
                text: qsTr("Kind")
                onTriggered: root.arrangeIconsBy("kind", false)
            }
        }

        DSStyle.MenuItem {
            text: qsTr("Clean Up Desktop")
            onTriggered: root.cleanUpDesktop()
        }

        DSStyle.MenuSeparator {}

        // ── Group 4: Desktop settings ─────────────────────────────────────────
        DSStyle.MenuItem {
            text: qsTr("Change Desktop Wallpaper…")
            onTriggered: root.changeWallpaper()
        }
        DSStyle.MenuItem {
            text: qsTr("Show View Options…")
            onTriggered: root.openAppearance()
        }

        DSStyle.MenuSeparator {}

        // ── Group 5: Utility ──────────────────────────────────────────────────
        DSStyle.MenuItem {
            text: qsTr("New Terminal at Desktop")
            onTriggered: root.openTerminal()
        }
    }
}
