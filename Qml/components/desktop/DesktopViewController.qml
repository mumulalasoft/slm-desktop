import QtQuick 2.15

QtObject {
    id: root

    property string path: "~/Desktop"
    property string mode: "desktop_view"
    property var items: []
    property var selection: []
    property var selectionIndexes: []
    property int selectionCount: selection.length
    property var menuContext: ({
        "selectionCount": 0,
        "selectionTypes": []
    })

    signal selectionChanged(var selection)
    signal directoryChanged(string path)

    function _isExecutable(row) {
        var mime = String(row && row.mimeType ? row.mimeType : "").toLowerCase()
        if (mime.indexOf("application/x-executable") >= 0) {
            return true
        }
        var suffix = String(row && row.suffix ? row.suffix : "").toLowerCase()
        return suffix === "sh" || suffix === "run" || suffix === "appimage"
    }

    function _toFileItem(row) {
        var isDir = !!(row && row.isDir)
        return {
            "path": String(row && row.path ? row.path : ""),
            "name": String(row && row.name ? row.name : ""),
            "type": isDir ? "dir" : "file",
            "icon": String(row && row.iconName ? row.iconName : ""),
            "isExecutable": _isExecutable(row),
            "isHidden": !!(row && row.hidden)
        }
    }

    function _selectionTypes(rows) {
        var out = []
        var hasDir = false
        var hasFile = false
        var src = rows ? rows : []
        for (var i = 0; i < src.length; ++i) {
            var t = String(src[i] && src[i].type ? src[i].type : "")
            if (t === "dir" && !hasDir) {
                hasDir = true
                out.push("dir")
            } else if (t === "file" && !hasFile) {
                hasFile = true
                out.push("file")
            }
        }
        return out
    }

    function setPath(pathValue) {
        var next = String(pathValue || "").trim()
        if (next.length <= 0) {
            next = "~/Desktop"
        }
        if (path === next) {
            return
        }
        path = next
        directoryChanged(path)
    }

    function setItems(rows) {
        var src = rows ? rows : []
        var out = []
        for (var i = 0; i < src.length; ++i) {
            out.push(_toFileItem(src[i]))
        }
        items = out
    }

    function setSelection(indexes, rows) {
        var src = rows ? rows : []
        var out = []
        for (var i = 0; i < src.length; ++i) {
            out.push(_toFileItem(src[i]))
        }
        selectionIndexes = indexes ? indexes : []
        selection = out
        menuContext = {
            "selectionCount": Number(out.length || 0),
            "selectionTypes": _selectionTypes(out)
        }
        selectionChanged(selection)
    }
}
