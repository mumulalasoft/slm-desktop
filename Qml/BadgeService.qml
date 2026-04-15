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

    function _normalizeToken(value) {
        var s = String(value || "").trim().toLowerCase()
        if (s.length === 0) {
            return ""
        }
        var slash = s.lastIndexOf("/")
        if (slash >= 0 && slash + 1 < s.length) {
            s = s.slice(slash + 1)
        }
        if (s.endsWith(".desktop")) {
            s = s.slice(0, s.length - 8)
        }
        s = s.replace(/\s+/g, ".")
        s = s.replace(/[^a-z0-9._-]/g, ".")
        s = s.replace(/\.{2,}/g, ".")
        s = s.replace(/^\.+/, "").replace(/\.+$/, "")
        return s
    }

    // Canonical identity key used by all badge operations.
    function canonicalKey(appId) {
        var key = _normalizeToken(appId)
        return key.length > 0 ? key : "unknown.app"
    }

    // Migrates pre-canonical keys already present in _counts.
    function migrateLegacyKeys() {
        var src = _counts || {}
        var dst = {}
        for (var k in src) {
            if (!Object.prototype.hasOwnProperty.call(src, k)) {
                continue
            }
            var canonical = canonicalKey(k)
            var c = Math.max(0, Math.round(Number(src[k]) || 0))
            if (c <= 0) {
                continue
            }
            var prev = Number(dst[canonical] || 0)
            dst[canonical] = Math.max(prev, c)
        }
        _counts = dst
    }

    // Set badge count for appId. count <= 0 clears the badge.
    function setBadge(appId, count) {
        var key = canonicalKey(appId)
        var c = Math.max(0, Math.round(Number(count) || 0))
        var copy = Object.assign({}, _counts)
        if (c <= 0) {
            delete copy[key]
        } else {
            copy[key] = c
        }
        _counts = copy
    }

    // Return current badge count for appId (0 if none).
    function getBadge(appId) {
        return Number(_counts[canonicalKey(appId)] || 0)
    }

    // Clear badge for appId.
    function clearBadge(appId) {
        setBadge(appId, 0)
    }

    // Clear all badges.
    function clearAll() {
        _counts = {}
    }

    Component.onCompleted: migrateLegacyKeys()
}
