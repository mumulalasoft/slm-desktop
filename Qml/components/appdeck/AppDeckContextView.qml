import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    property bool active: false
    property int panelHeight: 0
    property real preferredSurfaceX: -1
    property real preferredSurfaceY: -1
    property real preferredSurfaceWidth: -1
    property real preferredSurfaceHeight: -1

    // Required API for Pulse Context Mode
    property var pulseResultsModel: null
    property var appModelRef: null
    property var appDeckModelRef: null
    property string currentQuery: ""
    property int resultsGeneration: -1
    signal openResultRequested(string resultId)
    signal collapseRequested()
    signal queryChanged(string text)

    // Compatibility API for existing call sites
    property alias resultsModel: root.pulseResultsModel
    property alias queryText: root.currentQuery
    property int selectedIndex: -1
    property bool showDebugInfo: false
    property var searchProfileMeta: ({})
    property var telemetryMeta: ({})
    property var telemetryLast: ({})
    property var providerStats: ({})
    property var previewData: ({})

    signal dismissRequested()
    signal queryTextChangedRequest(string text)
    signal selectedIndexChangedByUser(int indexValue)
    signal resultActivated(int indexValue)
    signal resultContextAction(int indexValue, string action)

    property var bestMatch: null
    property var appsResults: []
    property var filesResults: []
    property var clipboardResults: []
    property var actionsResults: []
    property var recentResults: []
    property var suggestedResults: []
    property var navigationResults: []
    property string selectedResultId: ""
    property real revealProgress: active ? 1.0 : 0.0

    readonly property bool emptyQuery: String(root.currentQuery || "").trim().length <= 0
    readonly property bool modelHasRows: _countModel(root.pulseResultsModel) > 0
    readonly property bool hasResults: navigationResults.length > 0
    readonly property bool showEmptyState: emptyQuery && !modelHasRows
    readonly property bool showNoResultState: !emptyQuery && !modelHasRows
    readonly property bool showResultsState: hasResults
    readonly property bool showBestMatch: showResultsState && !emptyQuery && !!bestMatch && !showCalculatorPreviewCard
    readonly property var selectedResult: _resultById(selectedResultId)
    readonly property string selectedResultType: selectedResult ? String(selectedResult.type || "").toLowerCase() : ""
    readonly property string selectedResultSection: selectedResult ? String(selectedResult.section || "").toLowerCase() : ""
    readonly property bool selectedResultIsFile: selectedResult
                                                 && (selectedResultType === "file"
                                                     || selectedResultType === "path"
                                                     || selectedResultType === "folder"
                                                     || selectedResultType === "recent"
                                                     || selectedResultSection === "files"
                                                     || selectedResultSection === "recent")
    readonly property bool selectedResultIsClipboard: selectedResult
                                                      && (selectedResultType === "clipboard"
                                                          || selectedResultSection === "clipboard")
    readonly property bool selectedResultIsCalculator: selectedResult
                                                        && (selectedResultType === "calculator"
                                                            || selectedResultSection === "calculator")
    readonly property bool showFilePreviewCard: showResultsState && selectedResultIsFile
    readonly property bool showClipboardPreviewCard: showResultsState
                                                     && (selectedResultIsClipboard
                                                         || String((root.previewData && root.previewData.kind) || "").toLowerCase() === "clipboard")
    readonly property bool showCalculatorPreviewCard: showResultsState
                                                      && (selectedResultIsCalculator
                                                          || String((root.previewData && root.previewData.kind) || "").toLowerCase() === "calculator")
    readonly property real headerPhase: _phase(root.revealProgress, 0.0, 0.55)
    readonly property real bestPhase: _phase(root.revealProgress, 0.18, 0.72)
    readonly property real sectionsPhase: _phase(root.revealProgress, 0.32, 1.0)
    readonly property real footerPhase: _phase(root.revealProgress, 0.45, 1.0)
    readonly property int motionRevealDuration: Theme.durationNormal
    readonly property int motionFastDuration: Theme.durationFast
    readonly property int motionNormalDuration: Theme.durationNormal

    function focusSearchField() {
        if (queryHeader && queryHeader.focusInput) {
            queryHeader.focusInput()
        }
    }

    function _countModel(model) {
        if (!model) {
            return 0
        }
        return Number(model.count || 0)
    }

    function _phase(value, start, end) {
        var v = Number(value || 0)
        var s = Number(start || 0)
        var e = Number(end || 1)
        if (v <= s) return 0.0
        if (v >= e) return 1.0
        return (v - s) / Math.max(0.0001, (e - s))
    }

    function _calculatorPreviewHint(compoundLabel, calcKind) {
        var labelMap = ({
            "torque": "Torque result",
            "momentum": "Momentum result",
            "force": "Force result",
            "power": "Power result",
            "pressure": "Pressure result",
            "energy": "Energy result"
        })
        if (labelMap[compoundLabel]) {
            return labelMap[compoundLabel]
        }
        if (calcKind === "conversion") {
            return "Supports calc/convert, natural unit conversion, compound units like N*m, newton meter, kilogram meter per second, and J/s, joule per second, percent/ratio, angle/frequency, data rate like MBps and MiBps, acceleration, density, flow, force, temperature scales, pressure, energy, power, and ans"
        }
        return "Supports ans"
    }

    function _modelRow(model, index) {
        if (!model || !model.get) {
            return null
        }
        return model.get(index)
    }


    function _deriveType(row) {
        var raw = String((row && row.type) || "").toLowerCase()
        var kind = String((row && row.resultKind) || "").toLowerCase()
        var sectionTitle = String((row && row.sectionTitle) || "").toLowerCase()
        if (kind === "app") return "app"
        if (kind === "action") return "action"
        if (kind === "clipboard") return "clipboard"
        if (kind === "calculator") return "calculator"
        if (kind === "file" || kind === "folder") return "file"
        if (sectionTitle === "top apps") return "app"
        if (sectionTitle === "actions" || sectionTitle === "settings") return "action"
        if (sectionTitle === "recent files" || sectionTitle === "folders") return "file"
        if (raw === "desktop" || raw === "application") return "app"
        if (raw === "clipboard") return "clipboard"
        if (raw === "calculator") return "calculator"
        if (raw.length > 0) {
            return raw
        }
        if (row && row.isApp) return "app"
        if (row && row.isDir) return "folder"
        var provider = String((row && row.provider) || "").toLowerCase()
        if (provider === "apps") return "app"
        if (provider === "recent") return "recent"
        if (provider === "clipboard") return "clipboard"
        if (provider === "calculator") return "calculator"
        if (provider === "slm_actions" || provider === "settings" || provider === "global_menu") return "action"
        return "item"
    }

    function _deriveSection(row, typeValue) {
        var explicitSection = String((row && row.section) || "").toLowerCase()
        var sectionTitle = String((row && row.sectionTitle) || "").toLowerCase()
        if (sectionTitle === "top apps") return "apps"
        if (sectionTitle === "actions" || sectionTitle === "settings") return "actions"
        if (sectionTitle === "recent files" || sectionTitle === "folders") return "files"
        if (explicitSection.length > 0) {
            return explicitSection
        }
        if (row && row.isBestMatch) {
            return "best"
        }
        var provider = String((row && row.provider) || "").toLowerCase()
        if (provider === "recent") return "recent"
        if (provider === "clipboard") return "clipboard"
        if (provider === "calculator") return "best"
        if (typeValue === "app") return "apps"
        if (typeValue === "file" || typeValue === "folder" || typeValue === "path") return "files"
        if (typeValue === "clipboard") return "clipboard"
        if (typeValue === "calculator") return "best"
        if (typeValue === "action" || typeValue === "command") return "actions"
        return "suggestions"
    }

    function _buildAppIconCatalog() {
        var catalog = ({})
        function desktopBase(value) {
            var v = String(value || "").trim().toLowerCase()
            if (v.length <= 0) {
                return ""
            }
            if (v.indexOf("/") >= 0) {
                var parts = v.split("/")
                v = String(parts[parts.length - 1] || "")
            }
            if (v.length > 8 && v.slice(v.length - 8) === ".desktop") {
                v = v.slice(0, v.length - 8)
            }
            return v
        }
        function putKey(key, iconNameValue, iconSourceValue) {
            var k = String(key || "").trim().toLowerCase()
            if (k.length <= 0 || catalog[k]) {
                return
            }
            catalog[k] = {
                "iconName": String(iconNameValue || ""),
                "iconSource": String(iconSourceValue || "")
            }
        }
        function scanModel(modelObj, maxScan) {
            if (!modelObj) {
                return
            }
            var rows = []
            var limit = Math.max(64, Number(maxScan || 280))
            if (modelObj.page) {
                rows = modelObj.page(0, limit, "") || []
            } else if (modelObj.get && typeof modelObj.count !== "undefined") {
                var c = Math.min(limit, Number(modelObj.count || 0))
                for (var i = 0; i < c; ++i) {
                    var r = modelObj.get(i)
                    if (r) rows.push(r)
                }
            }
            for (var j = 0; j < rows.length; ++j) {
                var row = rows[j]
                if (!row) continue
                var did = String(row.desktopId || "")
                var dfile = String(row.desktopFile || "")
                var exe = String(row.executable || "")
                var name = String(row.name || row.display || "")
                var iconN = String(row.iconName || "")
                var iconS = String(row.iconSource || row.icon || "")
                putKey("did:" + did, iconN, iconS)
                putKey("dfile:" + dfile, iconN, iconS)
                putKey("exe:" + exe, iconN, iconS)
                putKey("name:" + name, iconN, iconS)
                putKey("base:" + desktopBase(did), iconN, iconS)
                putKey("base:" + desktopBase(dfile), iconN, iconS)
            }
        }

        if (root.appDeckModelRef) {
            scanModel(root.appDeckModelRef, 340)
        } else if (typeof AppDeckModel !== "undefined" && AppDeckModel) {
            scanModel(AppDeckModel, 340)
        }
        if (root.appModelRef) {
            scanModel(root.appModelRef, 340)
        } else if (typeof AppModel !== "undefined" && AppModel) {
            scanModel(AppModel, 340)
        }
        return catalog
    }

    function _lookupAppIcon(row, catalog) {
        if (!row || !catalog) return ({})
        var did = String(row.desktopId || "")
        var dfile = String(row.desktopFile || "")
        var exe = String(row.executable || "")
        var pathValue = String(row.path || "")
        var name = String(row.name || row.title || "")
        function desktopBase(value) {
            var v = String(value || "").trim().toLowerCase()
            if (v.length <= 0) {
                return ""
            }
            if (v.indexOf("/") >= 0) {
                var parts = v.split("/")
                v = String(parts[parts.length - 1] || "")
            }
            if (v.length > 8 && v.slice(v.length - 8) === ".desktop") {
                v = v.slice(0, v.length - 8)
            }
            return v
        }
        var pathBase = desktopBase(pathValue)
        var didBase = desktopBase(did)
        var dfileBase = desktopBase(dfile)
        var hit = catalog["did:" + did.toLowerCase()]
                  || catalog["dfile:" + dfile.toLowerCase()]
                  || catalog["exe:" + exe.toLowerCase()]
                  || catalog["base:" + pathBase]
                  || catalog["base:" + didBase]
                  || catalog["base:" + dfileBase]
                  || catalog["name:" + name.toLowerCase()]
        return hit ? hit : ({})
    }

    function _sanitizeIconName(rawValue) {
        var v = String(rawValue || "").trim()
        if (v.length <= 0) {
            return ""
        }
        var comma = v.indexOf(",")
        if (comma > 0) {
            v = v.slice(0, comma).trim()
        }
        var parts = v.split(/\s+/)
        for (var i = 0; i < parts.length; ++i) {
            var p = String(parts[i] || "").trim()
            if (p.length <= 0) continue
            if (p === "GThemedIcon" || p === "ThemedIcon" || p === ".") continue
            return p
        }
        return v
    }

    function _isExplicitIconSource(value) {
        var v = String(value || "").trim().toLowerCase()
        return v.indexOf("qrc:/") === 0
               || v.indexOf("file:/") === 0
               || v.indexOf("http://") === 0
               || v.indexOf("https://") === 0
               || v.indexOf("image://") === 0
               || v.indexOf("/") === 0
    }

    function _themeIconSource(iconNameValue) {
        var n = _sanitizeIconName(iconNameValue)
        if (n.length <= 0) {
            return ""
        }
        if (_isExplicitIconSource(n)) {
            return n
        }
        var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                   ? ThemeIconController.revision : 0)
        return "image://themeicon/" + n + "?v=" + rev
    }

    function _normalizeResult(row, sourceIndex, appCatalog) {
        var title = String((row && (row.title || row.name)) || "").trim()
        var subtitle = String((row && (row.subtitle || row.path || row.intent || row.sectionTitle)) || "").trim()
        var typeValue = _deriveType(row)
        var sectionValue = _deriveSection(row, typeValue)
        var rowIconRaw = String((row && row.icon) || "")
        var rowIconNameRaw = String((row && row.iconName) || "")
        var rowIconSourceRaw = String((row && row.iconSource) || "")
        // Follow AppHub icon routing:
        // 1) iconName -> theme icon
        // 2) explicit source from iconSource/icon
        // 3) non-explicit icon token -> theme icon
        var iconValue = _sanitizeIconName(rowIconNameRaw)
        var iconSourceValue = _isExplicitIconSource(rowIconSourceRaw) ? rowIconSourceRaw : ""
        if (iconSourceValue.length <= 0 && _isExplicitIconSource(rowIconRaw)) {
            iconSourceValue = rowIconRaw
        }
        if (iconValue.length <= 0 && iconSourceValue.length <= 0) {
            iconValue = _sanitizeIconName(rowIconRaw)
        }

        // App rows: prefer real app icon metadata from app catalog over symbolic fallbacks.
        if (typeValue === "app" || sectionValue === "apps") {
            var appIcon = _lookupAppIcon(row, appCatalog)
            var appIconName = _sanitizeIconName(String((appIcon && appIcon.iconName) || ""))
            var appIconSource = String((appIcon && appIcon.iconSource) || "")
            if (appIconSource.length > 0) {
                iconSourceValue = appIconSource
            }
            if (appIconName.length > 0) {
                iconValue = appIconName
            }
            if (iconValue.length <= 0) {
                iconValue = "application-x-executable-symbolic"
            }
        }

        // Keep robust fallback by default to theme icons for files/recent.
        if ((typeValue === "file" || typeValue === "path" || sectionValue === "files" || sectionValue === "recent")
                && iconValue.length <= 0) {
            iconValue = (row && row.isDir) ? "folder-symbolic" : "text-x-generic-symbolic"
        }
        if ((typeValue === "action" || typeValue === "command" || sectionValue === "actions")
                && iconValue.length <= 0) {
            iconValue = "system-run-symbolic"
        }
        if (iconSourceValue.length <= 0 && iconValue.length > 0) {
            iconSourceValue = _themeIconSource(iconValue)
        }
        // Hard guarantee: Pulse delegates must always receive a concrete iconSource.
        if (iconSourceValue.length <= 0) {
            if (typeValue === "app" || sectionValue === "apps") {
                iconSourceValue = "qrc:/icons/apphub.svg"
            } else if (typeValue === "action" || typeValue === "command" || sectionValue === "actions") {
                iconSourceValue = "qrc:/icons/dark/pulse.svg"
            } else {
                iconSourceValue = "qrc:/icons/logo.svg"
            }
        }

        var stableId = String((row && row.resultId) || "")
        if (stableId.length <= 0) {
            stableId = String((row && (row.desktopId || row.desktopFile || row.path || row.actionId || row.name)) || "")
        }
        if (stableId.length <= 0) {
            stableId = "row-" + sourceIndex
        }
        return {
            "resultId": stableId,
            "title": title.length > 0 ? title : "Untitled",
            "subtitle": subtitle,
            "icon": iconValue,
            "iconSource": iconSourceValue,
            "type": typeValue,
            "score": Number((row && row.score) || 0),
            "section": sectionValue,
            "actionId": String((row && row.actionId) || ""),
            "isBestMatch": !!(row && row.isBestMatch),
            "isCalculator": !!(row && row.isCalculator),
            "expression": String((row && row.expression) || ""),
            "resultText": String((row && row.resultText) || ""),
            "copyText": String((row && row.copyText) || ""),
            "clipboardType": String((row && row.clipboardType) || ""),
            "sourceApplication": String((row && row.sourceApplication) || ""),
            "timestamp": Number((row && row.timestamp) || 0),
            "timestampBucket": Number((row && row.timestampBucket) || 0),
            "isSensitive": !!(row && row.isSensitive),
            "compoundLabel": String((row && row.compoundLabel) || ""),
            "previewData": (row && row.preview) ? row.preview : ({}),
            "sourceIndex": Number(sourceIndex)
        }
    }

    function _groupResult(item,
                          appsBucket,
                          filesBucket,
                          clipboardBucket,
                          actionsBucket,
                          recentBucket,
                          suggestedBucket) {
        if (!item) {
            return
        }
        if (item.section === "apps") {
            appsBucket.push(item)
            return
        }
        if (item.section === "files") {
            filesBucket.push(item)
            return
        }
        if (item.section === "clipboard") {
            clipboardBucket.push(item)
            return
        }
        if (item.section === "actions") {
            actionsBucket.push(item)
            return
        }
        if (item.section === "recent") {
            recentBucket.push(item)
            return
        }
        suggestedBucket.push(item)
    }

    function _bestMatchPriority(item, queryText) {
        if (!item) {
            return -999999
        }
        var p = Number(item.score || 0)
        var q = String(queryText || "").trim().toLowerCase()
        var title = String(item.title || "").toLowerCase()
        var subtitle = String(item.subtitle || "").toLowerCase()
        var hay = (title + " " + subtitle).trim()
        var section = String(item.section || "")
        var typeValue = String(item.type || "")

        if (q.length > 0) {
            // Query relevance must dominate best-match pick.
            if (title === q) {
                p += 260
            } else if (title.indexOf(q) === 0) {
                p += 200
            } else if (title.indexOf(q) >= 0) {
                p += 140
            } else if (hay.indexOf(q) >= 0) {
                p += 70
            } else {
                p -= 220
            }

            // Intent bias for typed app names.
            if (typeValue === "app" || section === "apps") {
                p += 80
            } else if (typeValue === "action" || section === "actions") {
                p += 8
            } else if (typeValue === "file" || typeValue === "path" || section === "files") {
                p -= 12
            }

            if (item.isBestMatch) {
                p += 30
            }
            if (typeValue === "calculator" || section === "best") {
                p += 2600
            }
            return p
        }

        // Empty query: respect provider hint and keep apps discoverable.
        if (item.isBestMatch) p += 120
        if (section === "apps") p += 30
        return p
    }

    function _rebuildSections() {
        var rows = []
        var count = _countModel(root.pulseResultsModel)
        var appCatalog = _buildAppIconCatalog()
        for (var i = 0; i < count; ++i) {
            var row = _modelRow(root.pulseResultsModel, i)
            if (!row) {
                continue
            }
            rows.push(_normalizeResult(row, i, appCatalog))
        }

        rows.sort(function(a, b) {
            return Number(b.score || 0) - Number(a.score || 0)
        })

        var nextBestMatch = null
        var nextApps = []
        var nextFiles = []
        var nextClipboard = []
        var nextActions = []
        var nextRecent = []
        var nextSuggested = []
        var nextNavigation = []

        if (rows.length > 0) {
            var bestIndex = 0
            var bestPriority = -999999
            for (var j = 0; j < rows.length; ++j) {
                var candidatePriority = _bestMatchPriority(rows[j], root.currentQuery)
                if (candidatePriority > bestPriority) {
                    bestPriority = candidatePriority
                    bestIndex = j
                }
            }
            nextBestMatch = rows[bestIndex]

            for (var k = 0; k < rows.length; ++k) {
                if (k === bestIndex) {
                    continue
                }
                _groupResult(rows[k],
                             nextApps,
                             nextFiles,
                             nextClipboard,
                             nextActions,
                             nextRecent,
                             nextSuggested)
            }

            nextNavigation.push(nextBestMatch)
            for (var a = 0; a < nextApps.length; ++a) nextNavigation.push(nextApps[a])
            for (var f = 0; f < nextFiles.length; ++f) nextNavigation.push(nextFiles[f])
            for (var c = 0; c < nextClipboard.length; ++c) nextNavigation.push(nextClipboard[c])
            for (var r = 0; r < nextRecent.length; ++r) nextNavigation.push(nextRecent[r])
            for (var ac = 0; ac < nextActions.length; ++ac) nextNavigation.push(nextActions[ac])
            for (var s = 0; s < nextSuggested.length; ++s) nextNavigation.push(nextSuggested[s])
        }

        bestMatch = nextBestMatch
        appsResults = nextApps
        filesResults = nextFiles
        clipboardResults = nextClipboard
        actionsResults = nextActions
        recentResults = nextRecent
        suggestedResults = nextSuggested
        navigationResults = nextNavigation

        if (navigationResults.length <= 0) {
            selectedResultId = ""
            selectedIndexChangedByUser(-1)
            console.log("[appdeck-pulse] rebuild query=\"" + String(root.currentQuery || "")
                        + "\" modelRows=" + Number(count || 0)
                        + " navRows=0")
            return
        }

        var keep = false
        for (var m = 0; m < navigationResults.length; ++m) {
            if (String(navigationResults[m].resultId) === selectedResultId) {
                keep = true
                break
            }
        }
        if (!keep) {
            selectedResultId = String(navigationResults[0].resultId || "")
        }
        _emitSelectedSourceIndex()
        console.log("[appdeck-pulse] rebuild query=\"" + String(root.currentQuery || "")
                    + "\" modelRows=" + Number(count || 0)
                    + " navRows=" + Number(navigationResults.length || 0)
                    + " apps=" + Number(appsResults.length || 0)
                    + " files=" + Number(filesResults.length || 0)
                    + " clipboard=" + Number(clipboardResults.length || 0)
                    + " recent=" + Number(recentResults.length || 0)
                    + " actions=" + Number(actionsResults.length || 0)
                    + " suggestions=" + Number(suggestedResults.length || 0))
    }

    function _selectedNavigationIndex() {
        for (var i = 0; i < navigationResults.length; ++i) {
            if (String(navigationResults[i].resultId || "") === selectedResultId) {
                return i
            }
        }
        return -1
    }

    function _sourceIndexForResult(resultId) {
        var id = String(resultId || "")
        for (var i = 0; i < navigationResults.length; ++i) {
            var item = navigationResults[i]
            if (String(item.resultId || "") === id) {
                return Number(item.sourceIndex)
            }
        }
        return -1
    }

    function _emitSelectedSourceIndex() {
        var index = _sourceIndexForResult(selectedResultId)
        selectedIndexChangedByUser(index)
    }

    function selectResult(resultId) {
        var id = String(resultId || "")
        if (id.length <= 0) {
            return
        }
        selectedResultId = id
        _emitSelectedSourceIndex()
    }

    function moveSelection(delta) {
        if (navigationResults.length <= 0) {
            return
        }
        var current = _selectedNavigationIndex()
        if (current < 0) {
            current = 0
        }
        var step = Number(delta || 0)
        if (step === 0) {
            return
        }
        var next = current + step
        while (next < 0) {
            next += navigationResults.length
        }
        next = next % navigationResults.length
        selectedResultId = String(navigationResults[next].resultId || "")
        _emitSelectedSourceIndex()
    }

    function _resultById(resultId) {
        var id = String(resultId || "")
        if (id.length <= 0) {
            return null
        }
        for (var i = 0; i < navigationResults.length; ++i) {
            var item = navigationResults[i]
            if (String(item.resultId || "") === id) {
                return item
            }
        }
        return null
    }

    function _rowsForSection(sectionName) {
        var target = String(sectionName || "")
        var out = []
        for (var i = 0; i < navigationResults.length; ++i) {
            var item = navigationResults[i]
            if (!item) {
                continue
            }
            if (String(item.section || "") === target) {
                out.push(item)
            }
        }
        return out
    }

    function _indexInSection(sectionName, resultId) {
        var rows = _rowsForSection(sectionName)
        var rid = String(resultId || "")
        for (var i = 0; i < rows.length; ++i) {
            if (String(rows[i].resultId || "") === rid) {
                return i
            }
        }
        return 0
    }

    function _pickFromSections(sectionNames, preferredPos) {
        var pos = Math.max(0, Number(preferredPos || 0))
        for (var i = 0; i < sectionNames.length; ++i) {
            var rows = _rowsForSection(sectionNames[i])
            if (rows.length <= 0) {
                continue
            }
            var idx = Math.min(rows.length - 1, pos)
            return rows[idx]
        }
        return null
    }

    function moveSelectionAcross(delta) {
        if (navigationResults.length <= 0) {
            return
        }
        var current = _resultById(selectedResultId)
        if (!current) {
            selectedResultId = String(navigationResults[0].resultId || "")
            _emitSelectedSourceIndex()
            return
        }

        var currentSection = String(current.section || "")
        var currentPos = _indexInSection(currentSection, current.resultId)
        var step = Number(delta || 0)
        if (step === 0) {
            return
        }

        var target = null
        if (step > 0) {
            // Left rail (apps/actions) -> Right rail (files/recent/suggestions)
            if (currentSection === "apps" || currentSection === "actions" || currentSection === "best") {
                target = _pickFromSections(["files", "clipboard", "recent", "suggestions"], currentPos)
            }
        } else {
            // Right rail (files/recent/suggestions) -> Left rail (apps/actions/best)
            if (currentSection === "files" || currentSection === "clipboard" || currentSection === "recent" || currentSection === "suggestions") {
                target = _pickFromSections(["apps", "actions", "best"], currentPos)
            }
        }

        if (!target) {
            return
        }
        selectedResultId = String(target.resultId || "")
        _emitSelectedSourceIndex()
    }

    function activateResult(resultId) {
        var id = String(resultId || "")
        if (id.length <= 0) {
            return
        }
        openResultRequested(id)
    }

    function activateSelected() {
        if (selectedResultId.length <= 0) {
            return
        }
        activateResult(selectedResultId)
    }

    function requestCollapse() {
        collapseRequested()
        dismissRequested()
    }

    anchors.fill: parent
    visible: active
    focus: active

    onCurrentQueryChanged: _rebuildSections()
    onPulseResultsModelChanged: _rebuildSections()
    onResultsGenerationChanged: _rebuildSections()
    onSelectedIndexChanged: {
        var target = Number(selectedIndex)
        if (target < 0) {
            return
        }
        for (var i = 0; i < navigationResults.length; ++i) {
            if (Number(navigationResults[i].sourceIndex) === target) {
                selectedResultId = String(navigationResults[i].resultId || "")
                break
            }
        }
    }
    Connections {
        target: root.pulseResultsModel || null
        ignoreUnknownSignals: true
        function onCountChanged() { root._rebuildSections() }
        function onDataChanged() { root._rebuildSections() }
        function onRowsInserted() { root._rebuildSections() }
        function onRowsRemoved() { root._rebuildSections() }
        function onModelReset() { root._rebuildSections() }
    }

    onVisibleChanged: {
        if (visible) {
            revealAnim.restart()
            Qt.callLater(function() {
                root.focusSearchField()
            })
        }
    }

    NumberAnimation {
        id: revealAnim
        target: root
        property: "revealProgress"
        from: 0.0
        to: 1.0
        duration: root.motionRevealDuration
        easing.type: Theme.easingDecelerate
    }

    Keys.onPressed: function(event) {
        if (!root.active) {
            return
        }
        if (event.key === Qt.Key_Escape) {
            root.requestCollapse()
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Up) {
            root.moveSelection(-1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Down) {
            root.moveSelection(1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Left) {
            root.moveSelectionAcross(-1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Right) {
            root.moveSelectionAcross(1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.activateSelected()
            event.accepted = true
        }
    }

    Rectangle {
        id: contextSurface
        width: root.preferredSurfaceWidth > 0
               ? root.preferredSurfaceWidth
               : Math.min(980, Math.max(620, root.width - 80))
        height: root.preferredSurfaceHeight > 0
                ? root.preferredSurfaceHeight
                : Math.max(420, root.height - (root.panelHeight + 74))
        x: root.preferredSurfaceX >= 0 ? root.preferredSurfaceX : Math.round((parent.width - width) * 0.5)
        y: (root.preferredSurfaceY >= 0 ? root.preferredSurfaceY : root.panelHeight + 16)
           + (1.0 - root.revealProgress) * 14
        radius: Math.max(18, Theme.radiusWindow + 4)
        color: "transparent"
        border.width: Theme.borderWidthNone
        border.color: "transparent"
        opacity: root.active ? 1.0 : 0.0
        scale: 0.985 + (0.015 * root.revealProgress)

        Behavior on opacity {
            NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingLight }
        }
        Behavior on y {
            NumberAnimation { duration: root.motionNormalDuration; easing.type: Theme.easingDecelerate }
        }
        Behavior on scale {
            NumberAnimation { duration: root.motionNormalDuration; easing.type: Theme.easingDecelerate }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: Theme.borderWidthNone
            border.color: "transparent"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Item {
                id: queryHeaderContainer
                Layout.fillWidth: true
                implicitHeight: queryHeader.implicitHeight
                opacity: root.headerPhase

                Behavior on opacity {
                    NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
                }

                PulseQueryHeader {
                    id: queryHeader
                    anchors.fill: parent
                    active: root.active
                    queryText: root.currentQuery
                    onQueryEdited: function(text) {
                        root.queryChanged(text)
                        root.queryTextChangedRequest(text)
                    }
                    onClearRequested: {
                        root.queryChanged("")
                        root.queryTextChangedRequest("")
                    }
                    onCloseRequested: root.requestCollapse()
                    onNavigate: function(delta) { root.moveSelection(delta) }
                    onNavigateSection: function(delta) { root.moveSelectionAcross(delta) }
                    onActivateCurrent: root.activateSelected()
                    onEscapePressed: root.requestCollapse()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 108 : 0
                visible: root.showClipboardPreviewCard
                radius: Theme.radiusCard
                color: Theme.color("windowCard")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("windowCardBorder")
                opacity: root.sectionsPhase

                Behavior on opacity {
                    NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingMd
                    spacing: Theme.spacingMd

                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: width * 0.5
                        color: Theme.color("shellIconPlateBg")
                        border.width: Theme.borderWidthNone

                        Image {
                            id: clipboardIcon
                            anchors.fill: parent
                            source: root.selectedResult ? String(root.selectedResult.iconSource || "") : ""
                            fillMode: Image.PreserveAspectFit
                            mipmap: false
                            smooth: true
                            visible: String(source).length > 0 && status !== Image.Error
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: !clipboardIcon.visible
                            text: "C"
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Theme.fontWeight("semibold")
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXxs

                        Label {
                            Layout.fillWidth: true
                            text: root.previewData && String(root.previewData.title || root.previewData.name || "").length > 0
                                  ? String(root.previewData.title || root.previewData.name || "")
                                  : (root.selectedResult ? String(root.selectedResult.title || "") : "")
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("body")
                            font.weight: Theme.fontWeight("semibold")
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.previewData && String(root.previewData.subtitle || "").length > 0
                                  ? String(root.previewData.subtitle || "")
                                  : (root.selectedResult ? String(root.selectedResult.subtitle || "") : "")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("xs")
                            elide: Text.ElideMiddle
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.previewData && String(root.previewData.preview || "").length > 0
                            text: String(root.previewData.preview || "")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }

                    Column {
                        spacing: Theme.spacingXxs

                        Label {
                            text: "Clipboard Preview"
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("xs")
                            font.weight: Theme.fontWeight("medium")
                            opacity: Theme.opacityMuted
                        }

                        Button {
                            text: "Paste"
                            enabled: root.selectedResultId.length > 0
                            onClicked: root.openResultRequested(root.selectedResultId)
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 112 : 0
                visible: root.showCalculatorPreviewCard
                radius: Theme.radiusCard
                color: Theme.color("windowCard")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("windowCardBorder")
                opacity: root.sectionsPhase

                Behavior on opacity {
                    NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingMd
                    spacing: Theme.spacingMd

                    Rectangle {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        radius: width * 0.5
                        color: Theme.color("accentSoft")
                        border.width: Theme.borderWidthNone

                        Label {
                            anchors.centerIn: parent
                            text: "="
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("bodyLarge")
                            font.weight: Theme.fontWeight("bold")
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXxs

                        Label {
                            Layout.fillWidth: true
                            text: root.previewData && String(root.previewData.result || "").length > 0
                                  ? String(root.previewData.result || "")
                                  : (root.selectedResult ? String(root.selectedResult.title || "") : "")
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("title")
                            font.weight: Theme.fontWeight("bold")
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.previewData && String(root.previewData.expression || "").length > 0
                                  ? String(root.previewData.expression || "")
                                  : (root.selectedResult ? String(root.selectedResult.subtitle || "") : "")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            elide: Text.ElideMiddle
                        }

                        Label {
                            Layout.fillWidth: true
                            text: {
                                var kind = String((root.previewData && root.previewData.kind) || "").toLowerCase()
                                if (kind === "calculator" && String((root.previewData && root.previewData.expression) || "").length > 0) {
                                    var compoundLabel = String((root.previewData && root.previewData.compoundLabel) || "").toLowerCase()
                                    var calcKind = String((root.previewData && root.previewData.calculatorKind) || "").toLowerCase()
                                    return _calculatorPreviewHint(compoundLabel, calcKind)
                                }
                                return "Enter copies the result"
                            }
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("xs")
                            opacity: Theme.opacityMuted
                        }
                    }

                    Column {
                        spacing: Theme.spacingXxs

                        Label {
                            text: "Calculator"
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("xs")
                            font.weight: Theme.fontWeight("medium")
                            opacity: Theme.opacityMuted
                        }

                        Button {
                            text: "Copy"
                            enabled: root.selectedIndex >= 0
                            onClicked: {
                                if (root.selectedIndex >= 0) {
                                    root.resultActivated(root.selectedIndex)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 72 : 0
                visible: root.showFilePreviewCard
                radius: Theme.radiusCard
                color: Theme.color("windowCard")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("windowCardBorder")
                opacity: root.sectionsPhase

                Behavior on opacity {
                    NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingMd
                    spacing: Theme.spacingMd

                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: width * 0.5
                        color: Theme.color("shellIconPlateBg")
                        border.width: Theme.borderWidthNone

                        Image {
                            id: selectedFileIcon
                            anchors.fill: parent
                            source: root.selectedResult ? String(root.selectedResult.iconSource || "") : ""
                            fillMode: Image.PreserveAspectFit
                            mipmap: false
                            smooth: true
                            visible: String(source).length > 0 && status !== Image.Error
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: !selectedFileIcon.visible
                            text: "F"
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Theme.fontWeight("semibold")
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXxs

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedResult ? String(root.selectedResult.title || "") : ""
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("body")
                            font.weight: Theme.fontWeight("semibold")
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedResult ? String(root.selectedResult.subtitle || "") : ""
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("xs")
                            elide: Text.ElideMiddle
                        }
                    }

                    Label {
                        text: "File Preview"
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Theme.fontWeight("medium")
                        opacity: Theme.opacityMuted
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Best Match"
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("semibold")
                visible: root.showBestMatch
                opacity: root.bestPhase * Theme.opacityMuted
            }

            PulseBestMatchCard {
                Layout.fillWidth: true
                visible: root.showBestMatch
                active: root.active
                revealAmount: root.bestPhase
                resultData: root.bestMatch
                selected: root.selectedResultId.length > 0
                          && root.bestMatch
                          && String(root.bestMatch.resultId || "") === root.selectedResultId
                onHovered: function(resultId) { root.selectResult(resultId) }
                onActivated: function(resultId) { root.activateResult(resultId) }
            }

            Item {
                id: sectionsContainer
                Layout.fillWidth: true
                Layout.fillHeight: true
                opacity: root.sectionsPhase

                Behavior on opacity {
                    NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
                }

                StackLayout {
                    anchors.fill: parent
                    currentIndex: root.showEmptyState
                                  ? 0
                                  : (root.showNoResultState ? 1 : 2)

                PulseEmptyState {
                    active: root.showEmptyState
                    onSuggestionChosen: function(text) {
                        root.queryChanged(text)
                        root.queryTextChangedRequest(text)
                    }
                }

                PulseNoResultState {
                    active: root.showNoResultState
                    queryText: root.currentQuery
                    onSuggestionChosen: function(text) {
                        root.queryChanged(text)
                        root.queryTextChangedRequest(text)
                    }
                }

                Flickable {
                    clip: true
                    interactive: root.showResultsState
                    contentWidth: width
                    contentHeight: dashboard.implicitHeight

                    PulseResultsDashboard {
                        id: dashboard
                        width: parent.width
                        active: root.showResultsState && root.active
                        layoutWidth: contextSurface.width
                        appsResults: root.appsResults
                        filesResults: root.filesResults
                        clipboardResults: root.clipboardResults
                        actionsResults: root.actionsResults
                        recentResults: root.recentResults
                        suggestedResults: root.suggestedResults
                        selectedResultId: root.selectedResultId
                        twoColumnThreshold: 620
                        onItemHovered: function(resultId) { root.selectResult(resultId) }
                        onItemActivated: function(resultId) { root.activateResult(resultId) }
                        onItemContextAction: function(resultId, action) {
                            var source = root._sourceIndexForResult(resultId)
                            if (source >= 0) {
                                root.resultContextAction(source, action)
                            }
                        }
                    }
                }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                opacity: Theme.opacitySubtle + ((Theme.opacityMuted - Theme.opacitySubtle) * root.footerPhase)

                Behavior on opacity {
                    NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingLight }
                }

                PulseFooterHints {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    active: root.active
                    emphasize: root.selectedResultId.length > 0
                }
            }
        }
    }

    Component.onCompleted: _rebuildSections()
}
