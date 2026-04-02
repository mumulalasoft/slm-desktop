// FileManagerSearchBridge.qml
//
// Bridge QML untuk in-process file search via FileManagerShellBridge.
// Menerima query dari shell search bar, mengembalikan hasil secara incremental.
//
// Cara pakai di shell search bar:
//   FileManagerSearchBridge {
//       id: fileSearch
//       rootPath: "/home"
//       onResultsChanged: searchResultsView.model = fileSearch.results
//   }
//   // Mulai search:
//   fileSearch.search("*.pdf")
//   // Cancel:
//   fileSearch.cancel()

import QtQuick

Item {
    id: root

    // Direktori awal pencarian (default: home dir)
    property string rootPath: Qt.resolvedUrl("file://" + StandardPaths.standardLocations(
        StandardPaths.HomeLocation)[0]).toString().replace("file://", "")

    // Hasil pencarian — list of { path, name, mimeType, size, modified }
    property var results: []

    // true selama pencarian berjalan
    property bool searching: false

    // Query terakhir
    property string lastQuery: ""

    // Minimum karakter sebelum pencarian dimulai
    property int minQueryLength: 2

    // Delay debounce dalam ms sebelum mulai search setelah keystroke
    property int debounceMs: 250

    // ── Signals ───────────────────────────────────────────────────────────

    signal searchStarted(string query)
    signal searchFinished(string query)
    signal resultsChanged()

    // ── Internal ──────────────────────────────────────────────────────────

    property string _currentSessionId: ""
    property int _pendingSearchCount: 0

    // ── API ───────────────────────────────────────────────────────────────

    /**
     * Mulai pencarian dengan debounce.
     * Panggilan cepat berturut-turut hanya mengeksekusi yang terakhir.
     */
    function search(query) {
        if (!query || query.length < root.minQueryLength) {
            cancel()
            root.results = []
            root.resultsChanged()
            return
        }
        root.lastQuery = query
        debounceTimer.restart()
    }

    /** Jalankan pencarian langsung tanpa debounce */
    function searchImmediate(query) {
        if (!query || query.length < root.minQueryLength) {
            cancel()
            root.results = []
            root.resultsChanged()
            return
        }
        _doSearch(query)
    }

    /** Batalkan pencarian yang sedang berjalan */
    function cancel() {
        if (root._currentSessionId && FileManagerShellBridge) {
            FileManagerShellBridge.cancelSearch(root._currentSessionId)
        }
        root._currentSessionId = ""
        root.searching = false
        debounceTimer.stop()
    }

    // ── Private ───────────────────────────────────────────────────────────

    function _doSearch(query) {
        cancel() // batalkan sesi sebelumnya
        if (typeof FileManagerShellBridge === "undefined" || !FileManagerShellBridge) {
            console.warn("[FileManagerSearchBridge] FileManagerShellBridge tidak tersedia")
            return
        }

        // Generate session ID unik
        root._currentSessionId = "search-" + Date.now() + "-" + Math.random().toString(36).substr(2, 5)
        root.searching = true
        root.results = []
        root.searchStarted(query)

        FileManagerShellBridge.searchFiles(root.rootPath, query, root._currentSessionId)
    }

    // ── Debounce timer ────────────────────────────────────────────────────

    Timer {
        id: debounceTimer
        interval: root.debounceMs
        onTriggered: root._doSearch(root.lastQuery)
    }

    // ── Wiring ke bridge ──────────────────────────────────────────────────

    Connections {
        target: typeof FileManagerShellBridge !== "undefined"
                ? FileManagerShellBridge : null
        enabled: target !== null

        function onSearchResultsAvailable(sessionId, entries) {
            if (sessionId !== root._currentSessionId) return
            // Append incremental results
            root.results = root.results.concat(entries)
            root.resultsChanged()
        }

        function onSearchFinished(sessionId) {
            if (sessionId !== root._currentSessionId) return
            root.searching = false
            root._currentSessionId = ""
            root.searchFinished(root.lastQuery)
        }
    }
}
