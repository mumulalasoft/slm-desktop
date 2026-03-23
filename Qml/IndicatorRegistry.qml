pragma Singleton

import QtQuick 2.15

QtObject {
    id: root

    property var entries: []
    property int _nextId: 1

    signal changed()

    function _cloneList(list) {
        var out = []
        for (var i = 0; i < list.length; ++i) {
            out.push(list[i])
        }
        return out
    }

    function _sorted(list) {
        var out = _cloneList(list)
        out.sort(function(a, b) {
            var ao = (a && a.order !== undefined) ? Number(a.order) : 1000
            var bo = (b && b.order !== undefined) ? Number(b.order) : 1000
            if (ao === bo) {
                var an = a && a.name ? String(a.name) : ""
                var bn = b && b.name ? String(b.name) : ""
                return an.localeCompare(bn)
            }
            return ao - bo
        })
        return out
    }

    // spec:
    // {
    //   name: "my-indicator",
    //   source: "qrc:/my/Indicator.qml",
    //   order: 900,
    //   visible: true,
    //   enabled: true,
    //   properties: { ... }
    // }
    function registerIndicator(spec) {
        if (!spec || !spec.source) {
            return -1
        }
        var id = _nextId++
        var item = {
            id: id,
            name: spec.name ? String(spec.name) : ("indicator-" + id),
            source: String(spec.source),
            order: spec.order !== undefined ? Number(spec.order) : 1000,
            visible: spec.visible !== false,
            enabled: spec.enabled !== false,
            properties: spec.properties ? spec.properties : {}
        }
        var next = _cloneList(entries)
        next.push(item)
        entries = _sorted(next)
        changed()
        return id
    }

    function unregisterIndicator(id) {
        var next = []
        var removed = false
        for (var i = 0; i < entries.length; ++i) {
            var item = entries[i]
            if (item && Number(item.id) === Number(id)) {
                removed = true
                continue
            }
            next.push(item)
        }
        if (!removed) {
            return false
        }
        entries = _sorted(next)
        changed()
        return true
    }

    function clearExternalIndicators() {
        entries = []
        changed()
    }
}
