pragma Singleton
import QtQuick 2.15

// BadgeService — shell-wide notification badge registry.
//
// QML components call setBadge(appId, count) to set a badge count.
// DockAppDelegate reads getBadge(appId) (via the reactive _counts object) to
// display the red badge bubble on dock icons.
//
// Reactivity: _counts is always reassigned (never mutated in-place) so QML
// bindings on _counts or on getBadge() re-evaluate on every change.
QtObject {
    id: root

    // Internal map: appId → badge count (>0). Absent key means no badge.
    property var _counts: ({})

    // Set badge count for appId. count <= 0 clears the badge.
    function setBadge(appId, count) {
        var c = Math.max(0, Math.round(Number(count) || 0))
        var copy = Object.assign({}, _counts)
        if (c <= 0) {
            delete copy[String(appId)]
        } else {
            copy[String(appId)] = c
        }
        _counts = copy
    }

    // Return current badge count for appId (0 if none).
    function getBadge(appId) {
        return Number(_counts[String(appId)] || 0)
    }

    // Clear badge for appId.
    function clearBadge(appId) {
        setBadge(appId, 0)
    }

    // Clear all badges.
    function clearAll() {
        _counts = {}
    }
}
