import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as PulseComp

Item {
    id: root

    property bool active: false
    property bool compactWindow: false
    property string queryText: ""
    property var resultsModel: null
    property int selectedIndex: -1
    property bool showDebugInfo: false
    property var searchProfileMeta: ({})
    property var telemetryMeta: ({})
    property var telemetryLast: ({})
    property var providerStats: ({})
    property var previewData: ({})

    signal queryTextChangedRequest(string text)
    signal resultActivated(int index)
    signal resultContextAction(int index, string action)
    signal dismissRequested()

    QtObject {
        id: pulseController
        property bool isOpen: root.active
        property string searchText: root.queryText
        property string mode: "idle" // idle | search | command
        property int focusedSection: 0
        property int focusedItem: 0
        property var results: ({
                                   "top": [],
                                   "quick": [],
                                   "apps": [],
                                   "files": [],
                                   "system": []
                               })
    }

    readonly property bool commandMode: pulseController.mode === "command"
    readonly property bool searchMode: pulseController.mode === "search"
    readonly property bool idleMode: pulseController.mode === "idle"

    ListModel { id: topResultModel }
    ListModel { id: quickActionModel }
    ListModel { id: appModel }
    ListModel { id: fileModel }
    ListModel { id: systemModel }

    property string commandPreviewTitle: ""
    property string commandPreviewImpact: ""

    function focusSearch() {
        if (pulseInput && pulseInput.focusAndSelectAll) {
            pulseInput.focusAndSelectAll()
        }
    }

    function openFirstResultIfNeeded() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
            return
        }
        if (DesktopSettings.settingValue("pulse.enterOpensFirstResult", true) !== true) {
            return
        }
        if (root.selectedIndex >= 0) {
            return
        }
        if (topResultModel.count > 0) {
            pulseController.focusedSection = 0
            pulseController.focusedItem = 0
            syncFocusedToSelected()
        } else if (appModel.count > 0) {
            pulseController.focusedSection = 2
            pulseController.focusedItem = 0
            syncFocusedToSelected()
        } else if (fileModel.count > 0) {
            pulseController.focusedSection = 3
            pulseController.focusedItem = 0
            syncFocusedToSelected()
        } else if (systemModel.count > 0) {
            pulseController.focusedSection = 4
            pulseController.focusedItem = 0
            syncFocusedToSelected()
        }
    }

    function toCommandText(rawText) {
        var t = String(rawText || "").trim()
        if (t.length <= 0) return ""
        if (t.charAt(0) === ">" || t.charAt(0) === "/") {
            return t.substring(1).trim()
        }
        return ""
    }

    function updateMode() {
        var trimmed = String(root.queryText || "").trim()
        if (trimmed.length <= 0) {
            pulseController.mode = "idle"
            return
        }
        if (trimmed.charAt(0) === ">" || trimmed.charAt(0) === "/") {
            pulseController.mode = "command"
            return
        }
        pulseController.mode = "search"
    }

    function clearModels() {
        topResultModel.clear()
        quickActionModel.clear()
        appModel.clear()
        fileModel.clear()
        systemModel.clear()
        commandPreviewTitle = ""
        commandPreviewImpact = ""
    }

    function rowScore(row, qLower) {
        var name = String(row && row.name ? row.name : "")
        var nLower = name.toLowerCase()
        var exact = (qLower.length > 0 && nLower === qLower) ? 10000 : 0
        var keyword = (qLower.length > 0 && nLower.indexOf(qLower) >= 0) ? 1500 : 0
        var freq = Number(row && row.activationCount ? row.activationCount : 0)
        var recentMs = Number(row && row.lastOpenedMs ? row.lastOpenedMs : 0)
        var recent = recentMs > 0 ? Math.max(0, 1000 - Math.floor((Date.now() - recentMs) / 60000.0)) : 0
        var context = 0
        var provider = String(row && row.provider ? row.provider : "")
        if (provider === "apps") context += 120
        if (provider === "settings") context += 80
        if (provider === "slm_actions") context += 100
        if (provider === "calculator" || String(row && row.type ? row.type : "") === "calculator") {
            context += 22000
        }
        return exact + keyword + freq * 6 + recent + context
    }

    function toEntry(row, idx) {
        if (!row) return ({})
        var provider = String(row.provider || "")
        var type = String(row.type || "")
        var kind = String(row.resultKind || "")
        var isApp = !!row.isApp || type === "app" || provider === "apps" || String(row.desktopId || "").length > 0
        var isFileLike = !!row.isDir || kind === "file" || kind === "folder" || type === "path" || String(row.path || "").length > 0
        var isCalculator = provider === "calculator" || kind === "calculator" || type === "calculator"
        var isSystem = provider === "settings" || provider === "slm_actions" || kind === "settings" || kind === "action"
        return {
            "sourceIndex": Number(idx),
            "name": String(row.name || ""),
            "path": String(row.path || ""),
            "iconName": String(row.iconName || ""),
            "iconSource": String(row.iconSource || ""),
            "provider": provider,
            "type": type,
            "resultKind": kind,
            "desktopId": String(row.desktopId || ""),
            "desktopFile": String(row.desktopFile || ""),
            "isDir": !!row.isDir,
            "isApp": isApp,
            "isFileLike": isFileLike,
            "isCalculator": isCalculator,
            "isSystem": isSystem
        }
    }

    function appendTopActions(entry) {
        if (!entry || entry.sourceIndex < 0) return
        if (entry.isApp) {
            quickActionModel.append({ "label": "Open", "action": "open", "sourceIndex": entry.sourceIndex })
            quickActionModel.append({ "label": "Pin to AppDeck", "action": "pinToAppDeck", "sourceIndex": entry.sourceIndex })
            quickActionModel.append({ "label": "Open New Window", "action": "launch", "sourceIndex": entry.sourceIndex })
            return
        }
        if (entry.isCalculator) {
            quickActionModel.append({ "label": "Copy Result", "action": "copyResult", "sourceIndex": entry.sourceIndex })
            quickActionModel.append({ "label": "Copy Expression", "action": "copyExpression", "sourceIndex": entry.sourceIndex })
            return
        }
        if (entry.isSystem) {
            quickActionModel.append({ "label": "Execute", "action": "open", "sourceIndex": entry.sourceIndex })
            return
        }
        quickActionModel.append({ "label": "Open", "action": "open", "sourceIndex": entry.sourceIndex })
        quickActionModel.append({ "label": "Reveal", "action": "openContainingFolder", "sourceIndex": entry.sourceIndex })
        quickActionModel.append({ "label": "Copy Path", "action": "copyPath", "sourceIndex": entry.sourceIndex })
    }

    function buildIdleQuickActions() {
        quickActionModel.append({ "label": "Toggle Dark Mode", "command": "toggle dark mode" })
        quickActionModel.append({ "label": "Toggle Bluetooth", "command": "toggle bluetooth" })
        quickActionModel.append({ "label": "Open Settings", "command": "open settings" })
        quickActionModel.append({ "label": "Open AppDeck", "command": "open appdeck" })
        quickActionModel.append({ "label": "AppDeck: Utilities", "command": "open appdeck utilities" })
    }

    function executeCommand(commandText) {
        var c = String(commandText || "").trim().toLowerCase()
        if (c.length <= 0) return false

        if (c === "toggle dark mode" || c === "dark mode" || c === "theme dark") {
            if (typeof Theme !== "undefined" && Theme && Theme.toggleMode) {
                Theme.toggleMode()
                return true
            }
            return false
        }
        if (c === "toggle bluetooth" || c === "bluetooth off" || c === "bluetooth on") {
            if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.setSettingValue) {
                var cur = !!DesktopSettings.settingValue("bluetooth.enabled", false)
                if (c === "bluetooth off") cur = true
                if (c === "bluetooth on") cur = false
                DesktopSettings.setSettingValue("bluetooth.enabled", !cur)
                return true
            }
            return false
        }
        if (c === "open settings") {
            if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
                AppCommandRouter.route("app.desktopid", { "desktopId": "slm-settings.desktop" }, "pulse")
                return true
            }
            return false
        }
        if (c === "open appdeck" || c.indexOf("open appdeck ") === 0) {
            var appDeckSeed = ""
            if (c.indexOf("open appdeck ") === 0) {
                appDeckSeed = String(c.substring("open appdeck ".length) || "").trim()
            }
            if (typeof ShellStateController !== "undefined"
                    && ShellStateController
                    && ShellStateController.setAppDeckSearchSeed) {
                ShellStateController.setAppDeckSearchSeed(appDeckSeed)
            }
            if (typeof ShellStateController !== "undefined" && ShellStateController && ShellStateController.setAppDeckVisible) {
                ShellStateController.setAppDeckVisible(true)
                return true
            }
            return false
        }
        if (c === "shutdown") {
            if (typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.shutdown) {
                PowerBridge.shutdown()
                return true
            }
            return false
        }
        if (c === "reboot") {
            if (typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.reboot) {
                PowerBridge.reboot()
                return true
            }
            return false
        }
        if (c === "wifi off" || c === "wifi on") {
            // Not wired directly yet; keep command discoverable and route user to settings.
            if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
                AppCommandRouter.route("app.desktopid", { "desktopId": "slm-settings.desktop" }, "pulse")
                return true
            }
            return false
        }
        return false
    }

    function restoreFocusBySource(sourceIndex) {
        var idx = Number(sourceIndex)
        if (idx < 0) return false
        if (topResultModel.count > 0 && Number(topResultModel.get(0).sourceIndex) === idx) {
            pulseController.focusedSection = 0
            pulseController.focusedItem = 0
            root.selectedIndex = idx
            return true
        }
        for (var i = 0; i < appModel.count; ++i) {
            if (Number(appModel.get(i).sourceIndex) === idx) {
                pulseController.focusedSection = 2
                pulseController.focusedItem = i
                root.selectedIndex = idx
                return true
            }
        }
        for (var j = 0; j < fileModel.count; ++j) {
            if (Number(fileModel.get(j).sourceIndex) === idx) {
                pulseController.focusedSection = 3
                pulseController.focusedItem = j
                root.selectedIndex = idx
                return true
            }
        }
        for (var k = 0; k < systemModel.count; ++k) {
            if (Number(systemModel.get(k).sourceIndex) === idx) {
                pulseController.focusedSection = 4
                pulseController.focusedItem = k
                root.selectedIndex = idx
                return true
            }
        }
        return false
    }

    function buildCommandModeSections() {
        var cmd = toCommandText(root.queryText)
        if (cmd.length <= 0) {
            commandPreviewTitle = "Command mode"
            commandPreviewImpact = "Type a command, e.g. > open settings"
            return
        }

        var supported = [
            { "name": "open settings", "impact": "Open SLM Settings" },
            { "name": "open appdeck", "impact": "Show AppDeck overlay" },
            { "name": "open appdeck utilities", "impact": "Open AppDeck filtered by utilities" },
            { "name": "open appdeck internet", "impact": "Open AppDeck filtered by internet apps" },
            { "name": "open appdeck graphics", "impact": "Open AppDeck filtered by graphics apps" },
            { "name": "toggle dark mode", "impact": "Toggle light/dark theme" },
            { "name": "toggle bluetooth", "impact": "Toggle bluetooth.enabled" },
            { "name": "wifi off", "impact": "Route to network control" },
            { "name": "wifi on", "impact": "Route to network control" },
            { "name": "shutdown", "impact": "Power off device" },
            { "name": "reboot", "impact": "Restart device" }
        ]

        var cmdLower = cmd.toLowerCase()
        for (var i = 0; i < supported.length; ++i) {
            var item = supported[i]
            var name = String(item.name)
            if (name.indexOf(cmdLower) >= 0 || cmdLower.indexOf(name) >= 0) {
                systemModel.append({
                                       "label": name,
                                       "impact": String(item.impact),
                                       "command": name
                                   })
            }
        }

        if (systemModel.count > 0) {
            commandPreviewTitle = String(systemModel.get(0).label)
            commandPreviewImpact = String(systemModel.get(0).impact)
        } else {
            commandPreviewTitle = "> " + cmd
            commandPreviewImpact = "Command not recognized"
        }
    }

    function rebuildSections() {
        var prevSelected = Number(root.selectedIndex)
        var prevSection = Number(pulseController.focusedSection)
        var prevItem = Number(pulseController.focusedItem)

        updateMode()
        clearModels()

        var count = resultsModel ? Number(resultsModel.count || 0) : 0
        var qLower = String(root.queryText || "").trim().toLowerCase()
        var rows = []
        for (var i = 0; i < count; ++i) {
            var row = resultsModel.get(i)
            if (!row) continue
            var e = toEntry(row, i)
            e._score = rowScore(row, qLower)
            rows.push(e)
        }
        rows.sort(function(a, b) { return Number(b._score || 0) - Number(a._score || 0) })

        if (pulseController.mode === "command") {
            buildCommandModeSections()
            pulseController.results = ({ "top": [], "quick": [], "apps": [], "files": [], "system": systemModel.count })
            pulseController.focusedSection = 4
            pulseController.focusedItem = Math.max(0, Math.min(systemModel.count - 1, prevItem))
            syncCommandPreviewToFocus()
            root.selectedIndex = -1
            return
        }

        if (rows.length > 0) {
            var top = rows[0]
            topResultModel.append(top)
            appendTopActions(top)
        } else if (pulseController.mode === "idle") {
            buildIdleQuickActions()
        }

        for (var j = 0; j < rows.length; ++j) {
            var r = rows[j]
            if (j > 0 && r.isApp && appModel.count < 8) {
                appModel.append(r)
            } else if (r.isFileLike && fileModel.count < 8) {
                fileModel.append(r)
            } else if (r.isSystem && systemModel.count < 8) {
                systemModel.append(r)
            }
        }

        pulseController.results = ({
                                       "top": topResultModel.count,
                                       "quick": quickActionModel.count,
                                       "apps": appModel.count,
                                       "files": fileModel.count,
                                       "system": systemModel.count
                                   })

        if (!restoreFocusBySource(prevSelected)) {
            pulseController.focusedSection = prevSection
            pulseController.focusedItem = prevItem
            if (sectionCount(pulseController.focusedSection) <= 0) {
                pulseController.focusedSection = sectionNext(0, 1)
                pulseController.focusedItem = 0
            } else {
                pulseController.focusedItem = Math.max(0, Math.min(sectionCount(pulseController.focusedSection) - 1,
                                                                    pulseController.focusedItem))
            }
            syncFocusedToSelected()
            if (root.selectedIndex < 0 && topResultModel.count > 0) {
                root.selectedIndex = Number(topResultModel.get(0).sourceIndex)
            }
        }
    }

    function sectionCount(sectionId) {
        if (sectionId === 0) return topResultModel.count
        if (sectionId === 1) return quickActionModel.count
        if (sectionId === 2) return appModel.count
        if (sectionId === 3) return fileModel.count
        if (sectionId === 4) return systemModel.count
        return 0
    }

    function sectionNext(current, dir) {
        var step = dir >= 0 ? 1 : -1
        for (var k = 0; k < 6; ++k) {
            current += step
            if (current < 0) current = 4
            if (current > 4) current = 0
            if (sectionCount(current) > 0) return current
        }
        return current
    }

    function focusedSourceIndex() {
        var s = pulseController.focusedSection
        var i = pulseController.focusedItem
        if (s === 0 && topResultModel.count > i) return Number(topResultModel.get(i).sourceIndex)
        if (s === 2 && appModel.count > i) return Number(appModel.get(i).sourceIndex)
        if (s === 3 && fileModel.count > i) return Number(fileModel.get(i).sourceIndex)
        if (s === 4 && systemModel.count > i && !commandMode) return Number(systemModel.get(i).sourceIndex)
        return -1
    }

    function syncFocusedToSelected() {
        root.selectedIndex = focusedSourceIndex()
    }

    function syncCommandPreviewToFocus() {
        if (!commandMode || systemModel.count <= 0) return
        var idx = Math.max(0, Math.min(systemModel.count - 1, Number(pulseController.focusedItem)))
        var row = systemModel.get(idx)
        commandPreviewTitle = String(row && row.label ? row.label : "")
        commandPreviewImpact = String(row && row.impact ? row.impact : "")
    }

    function invokeFocused(primaryAction) {
        var s = pulseController.focusedSection
        var i = pulseController.focusedItem

        if (commandMode && s === 4 && systemModel.count > i) {
            var cmd = String(systemModel.get(i).command || "")
            executeCommand(cmd)
            return
        }

        if (s === 1 && quickActionModel.count > i) {
            var qa = quickActionModel.get(i)
            if (qa.command !== undefined) {
                executeCommand(String(qa.command || ""))
                return
            }
            var qIdx = Number(qa.sourceIndex)
            var qAction = String(qa.action || "open")
            if (qIdx >= 0) {
                if (qAction === "open") {
                    resultActivated(qIdx)
                } else {
                    resultContextAction(qIdx, qAction)
                }
            }
            return
        }

        var sourceIdx = focusedSourceIndex()
        if (sourceIdx < 0) return
        if (primaryAction) {
            resultActivated(sourceIdx)
        } else {
            var alt = "openContainingFolder"
            if (pulseController.focusedSection === 2) {
                alt = "pinToAppDeck"
            }
            resultContextAction(sourceIdx, alt)
        }
    }

    onQueryTextChanged: rebuildSections()
    onResultsModelChanged: rebuildSections()
    Connections {
        target: root.resultsModel || null
        ignoreUnknownSignals: true
        function onCountChanged() { rebuildSections() }
    }
    Component.onCompleted: rebuildSections()
    onActiveChanged: {
        if (root.active && DesktopSettings && DesktopSettings.settingValue
                && DesktopSettings.settingValue("pulse.autoFocusOnOpen", true) === true) {
            Qt.callLater(function() { root.focusSearch() })
        }
    }

    Keys.onPressed: function(event) {
        if (!root.active) return

        if (event.key === Qt.Key_Escape) {
            dismissRequested()
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Tab
                && !(event.modifiers & Qt.ControlModifier)
                && !(event.modifiers & Qt.AltModifier)
                && !(event.modifiers & Qt.MetaModifier)) {
            pulseController.focusedSection = sectionNext(pulseController.focusedSection,
                                                        (event.modifiers & Qt.ShiftModifier) ? -1 : 1)
            pulseController.focusedItem = 0
            syncFocusedToSelected()
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Right) {
            if (pulseController.focusedSection === 0 && quickActionModel.count > 0) {
                pulseController.focusedSection = 1
                pulseController.focusedItem = 0
                syncFocusedToSelected()
                event.accepted = true
                return
            }
        }

        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            var c = sectionCount(pulseController.focusedSection)
            if (c > 0) {
                var delta = event.key === Qt.Key_Up ? -1 : 1
                var next = Number(pulseController.focusedItem) + delta
                if (next < 0) next = c - 1
                if (next >= c) next = 0
                pulseController.focusedItem = next
                syncCommandPreviewToFocus()
                syncFocusedToSelected()
            }
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            openFirstResultIfNeeded()
            invokeFocused(!(event.modifiers & Qt.ShiftModifier))
            event.accepted = true
            return
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color("overlay")
        opacity: compactWindow ? 0.0 : 1.0
    }

    PulseComp.PulseSurface {
        id: pulseSurface
        anchors.fill: parent
        compactWindow: root.compactWindow

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            PulseComp.PulseInput {
                id: pulseInput
                Layout.fillWidth: true
                commandMode: root.commandMode
                text: root.queryText
                onTextEdited: function(text) { root.queryTextChangedRequest(text) }
            }

            PulseComp.PulseTopResult {
                Layout.fillWidth: true
                visible: topResultModel.count > 0
                focused: pulseController.focusedSection === 0
                titleText: topResultModel.count > 0 ? String(topResultModel.get(0).name || "") : ""
                subtitleText: topResultModel.count > 0 ? String(topResultModel.get(0).path || "") : ""
            }

            PulseComp.PulseActionRow {
                Layout.fillWidth: true
                visible: quickActionModel.count > 0
                model: quickActionModel
                focusedIndex: pulseController.focusedSection === 1 ? pulseController.focusedItem : -1
                onActionTriggered: function(index) {
                    pulseController.focusedSection = 1
                    pulseController.focusedItem = index
                    invokeFocused(true)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                PulseComp.PulseListSection {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    titleText: "Apps"
                    model: appModel
                    focusedIndex: pulseController.focusedSection === 2 ? pulseController.focusedItem : -1
                    showPath: false
                    onItemTriggered: function(index) {
                        pulseController.focusedSection = 2
                        pulseController.focusedItem = index
                        syncFocusedToSelected()
                        invokeFocused(true)
                    }
                }

                PulseComp.PulseListSection {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    titleText: "Files"
                    model: fileModel
                    focusedIndex: pulseController.focusedSection === 3 ? pulseController.focusedItem : -1
                    showPath: true
                    onItemTriggered: function(index) {
                        pulseController.focusedSection = 3
                        pulseController.focusedItem = index
                        syncFocusedToSelected()
                        invokeFocused(true)
                    }
                }
            }

            PulseComp.PulseCommandPreview {
                Layout.fillWidth: true
                height: 98
                visible: commandMode
                titleText: "Command Preview"
                previewTitle: commandPreviewTitle
                previewImpact: commandPreviewImpact
            }

            PulseComp.PulseGridSection {
                Layout.fillWidth: true
                height: 86
                visible: systemModel.count > 0
                titleText: commandMode ? "Commands" : "System / Commands"
                previewStyle: true
                model: systemModel
                focusedIndex: pulseController.focusedSection === 4 ? pulseController.focusedItem : -1
                onItemTriggered: function(index) {
                    pulseController.focusedSection = 4
                    pulseController.focusedItem = index
                    syncFocusedToSelected()
                    invokeFocused(true)
                }
            }
        }
    }
}
