import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

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

    function focusSearch() {
        queryField.forceActiveFocus()
        queryField.selectAll()
    }

    function moveSelection(delta) {
        var count = resultsModel ? Number(resultsModel.count || 0) : 0
        if (count <= 0) {
            selectedIndex = -1
            closeContextMenu()
            return
        }
        if (resultContextMenu.visible) {
            closeContextMenu()
        }
        var next = selectedIndex
        if (next < 0) {
            next = 0
        } else {
            next = Math.max(0, Math.min(count - 1, next + delta))
        }
        selectedIndex = next
        listView.positionViewAtIndex(selectedIndex, ListView.Contain)
    }

    function sectionAtIndex(indexValue) {
        var idx = Number(indexValue)
        var count = resultsModel ? Number(resultsModel.count || 0) : 0
        if (!resultsModel || idx < 0 || idx >= count) {
            return ""
        }
        var row = resultsModel.get(idx)
        return row ? String(row.sectionTitle || "") : ""
    }

    function moveSelectionByGroup(delta) {
        var count = resultsModel ? Number(resultsModel.count || 0) : 0
        if (!resultsModel || count <= 0) {
            selectedIndex = -1
            closeContextMenu()
            return
        }
        closeContextMenu()
        var dir = Number(delta) < 0 ? -1 : 1
        var cur = Number(selectedIndex)
        if (cur < 0 || cur >= count) {
            cur = 0
        }
        var currentSection = sectionAtIndex(cur)
        var target = -1
        if (dir > 0) {
            for (var i = cur + 1; i < count; ++i) {
                if (sectionAtIndex(i) !== currentSection) {
                    target = i
                    break
                }
            }
            if (target < 0) {
                target = 0
            }
        } else {
            for (var j = cur - 1; j >= 0; --j) {
                if (sectionAtIndex(j) !== currentSection) {
                    target = j
                    break
                }
            }
            if (target < 0) {
                target = count - 1
            }
            var targetSection = sectionAtIndex(target)
            while (target > 0 && sectionAtIndex(target - 1) === targetSection) {
                target--
            }
        }
        selectedIndex = Math.max(0, Math.min(count - 1, target))
        listView.positionViewAtIndex(selectedIndex, ListView.Contain)
    }

    function activateCurrent() {
        if (selectedIndex < 0) {
            return
        }
        closeContextMenu()
        resultActivated(selectedIndex)
    }

    function openContextMenuForIndex(indexValue, xPos, yPos) {
        var idx = Number(indexValue)
        if (!resultsModel || idx < 0 || idx >= Number(resultsModel.count || 0)) {
            return
        }
        var row = resultsModel.get(idx)
        if (!row) {
            return
        }
        root.selectedIndex = idx
        root.contextMenuIndex = idx
        root.contextMenuIsApp = (String(row.type || "") === "app")
                                || (String(row.provider || "") === "apps")
                                || String(row.desktopId || "").length > 0
                                || String(row.executable || "").length > 0
        root.contextMenuIsSettings = (String(row.provider || "") === "settings")
                                     || (String(row.type || "") === "settings")
                                     || (String(row.type || "") === "module")
                                     || (String(row.type || "") === "setting")
                                     || (String(row.resultKind || "") === "settings")
        root.contextMenuIsDir = !!row.isDir
        root.contextMenuHasPath = String(row.path || "").length > 0
        root.contextMenuActionIndex = 0
        resultContextMenu.openAt(xPos, yPos)
    }

    function openContextMenuForIndexGlobal(indexValue, globalX, globalY) {
        var lx = Number(globalX)
        var ly = Number(globalY)
        if (root.mapFromGlobal) {
            var p = root.mapFromGlobal(lx, ly)
            lx = Number(p.x)
            ly = Number(p.y)
        }
        openContextMenuForIndex(indexValue, lx, ly)
    }

    function closeContextMenu() {
        root.contextMenuActionIndex = -1
        resultContextMenu.closeMenu()
    }

    function formatBytes(bytesValue) {
        var b = Number(bytesValue || 0)
        if (!(b > 0)) {
            return "0 B"
        }
        var units = ["B", "KB", "MB", "GB", "TB"]
        var idx = 0
        while (b >= 1024 && idx < units.length - 1) {
            b /= 1024
            idx++
        }
        return (idx === 0 ? String(Math.round(b)) : b.toFixed(1)) + " " + units[idx]
    }

    function escapeHtml(value) {
        var s = String(value || "")
        s = s.replace(/&/g, "&amp;")
        s = s.replace(/</g, "&lt;")
        s = s.replace(/>/g, "&gt;")
        s = s.replace(/"/g, "&quot;")
        s = s.replace(/'/g, "&#39;")
        return s
    }

    function escapeRegex(value) {
        return String(value || "").replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
    }

    function highlightQueryText(titleValue, queryValue) {
        if (typeof TothespotTextHighlighter !== "undefined"
                && TothespotTextHighlighter
                && TothespotTextHighlighter.highlightStyledText) {
            return String(TothespotTextHighlighter.highlightStyledText(String(titleValue || ""),
                                                                       String(queryValue || ""),
                                                                       String(Theme.color("accent"))))
        }
        var title = String(titleValue || "")
        var query = String(queryValue || "").trim()
        if (query.length <= 0) {
            return escapeHtml(title)
        }
        var escapedTitle = escapeHtml(title)
        var re = new RegExp("(" + escapeRegex(query) + ")", "ig")
        var accent = String(Theme.color("accent"))
        return escapedTitle.replace(re,
                                    "<span style=\"font-weight:700;color:"
                                    + accent + ";\">$1</span>")
    }

    function formatProviderStats(statsValue) {
        var stats = statsValue || ({})
        var ordered = ["apps", "settings", "slm_actions", "clipboard", "recent", "tracker"]
        var parts = []
        for (var i = 0; i < ordered.length; ++i) {
            var key = ordered[i]
            if (stats[key] !== undefined) {
                parts.push(key + ":" + String(stats[key]))
            }
        }
        for (var k in stats) {
            if (ordered.indexOf(String(k)) >= 0) {
                continue
            }
            parts.push(String(k) + ":" + String(stats[k]))
        }
        return parts.join(", ")
    }

    property int contextMenuIndex: -1
    property bool contextMenuIsApp: false
    property bool contextMenuIsSettings: false
    property bool contextMenuIsDir: false
    property bool contextMenuHasPath: false
    property double contextMenuOpenedAtMs: 0
    property int contextMenuActionIndex: -1
    onWidthChanged: closeContextMenu()
    onHeightChanged: closeContextMenu()

    function contextMenuHasContaining() {
        if (typeof TothespotContextMenuHelper !== "undefined"
                && TothespotContextMenuHelper
                && TothespotContextMenuHelper.hasContainingAction) {
            return !!TothespotContextMenuHelper.hasContainingAction(root.contextMenuIsApp,
                                                                    root.contextMenuIsDir,
                                                                    root.contextMenuHasPath)
        }
        return !root.contextMenuIsApp && !root.contextMenuIsDir && root.contextMenuHasPath
    }

    function contextMenuHasCopyPath() {
        if (typeof TothespotContextMenuHelper !== "undefined"
                && TothespotContextMenuHelper
                && TothespotContextMenuHelper.hasCopyPathAction) {
            return !!TothespotContextMenuHelper.hasCopyPathAction(root.contextMenuIsApp,
                                                                  root.contextMenuIsDir,
                                                                  root.contextMenuHasPath)
        }
        return !root.contextMenuIsApp && root.contextMenuHasPath
    }

    function contextMenuVisibleActionCount() {
        if (root.contextMenuIsSettings) {
            return 3
        }
        if (typeof TothespotContextMenuHelper !== "undefined"
                && TothespotContextMenuHelper
                && TothespotContextMenuHelper.visibleActionCount) {
            return Number(TothespotContextMenuHelper.visibleActionCount(root.contextMenuIsApp,
                                                                        root.contextMenuIsDir,
                                                                        root.contextMenuHasPath))
        }
        if (contextMenuHasContaining()) return 4
        if (contextMenuHasCopyPath()) return 3
        return 2
    }

    function contextMenuActionForSlot(slot) {
        if (root.contextMenuIsSettings) {
            var ss = Number(slot)
            if (ss <= 0) return "openSetting"
            if (ss === 1) return "copyDeepLink"
            return "properties"
        }
        if (typeof TothespotContextMenuHelper !== "undefined"
                && TothespotContextMenuHelper
                && TothespotContextMenuHelper.actionForSlot) {
            return String(TothespotContextMenuHelper.actionForSlot(Number(slot),
                                                                   root.contextMenuIsApp,
                                                                   root.contextMenuIsDir,
                                                                   root.contextMenuHasPath))
        }
        var s = Number(slot)
        if (s <= 0) return root.contextMenuIsApp ? "launch" : "open"
        if (contextMenuHasContaining()) {
            if (s === 1) return "openContainingFolder"
            if (s === 2) return "copyPath"
            return "properties"
        }
        if (contextMenuHasCopyPath()) {
            if (s === 1) return "copyPath"
            return "properties"
        }
        return "properties"
    }

    function contextMenuLabelForAction(action) {
        var a = String(action || "")
        if (a === "openSetting") return "Open Setting"
        if (a === "copyDeepLink") return "Copy Deep Link"
        if (a === "launch") return "Launch"
        if (a === "open") return "Open"
        if (a === "openContainingFolder") return "Open Containing Folder"
        if (a === "copyPath") return "Copy Path"
        if (a === "copyName") return "Copy Name"
        if (a === "copyUri") return "Copy URI"
        if (a === "properties") return "Properties"
        return a
    }

    function contextMenuShortcutForAction(action) {
        var a = String(action || "")
        if (a === "openSetting") return "Enter"
        if (a === "copyDeepLink") return "Ctrl+Shift+C"
        if (a === "copyPath") return "Ctrl+C"
        if (a === "copyName") return "Ctrl+Alt+C"
        if (a === "copyUri") return "Ctrl+Shift+C"
        if (a === "openContainingFolder") return "Ctrl+Enter"
        if (a === "properties") return "Alt+Enter"
        return ""
    }

    function moveContextMenuSelection(delta) {
        if (!root.contextMenuIsSettings
                && typeof TothespotContextMenuHelper !== "undefined"
                && TothespotContextMenuHelper
                && TothespotContextMenuHelper.moveSelection) {
            root.contextMenuActionIndex = Number(TothespotContextMenuHelper.moveSelection(
                                                     root.contextMenuActionIndex,
                                                     Number(delta),
                                                     root.contextMenuIsApp,
                                                     root.contextMenuIsDir,
                                                     root.contextMenuHasPath))
            return
        }
        var count = contextMenuVisibleActionCount()
        if (count <= 0) { root.contextMenuActionIndex = -1; return }
        var next = Number(root.contextMenuActionIndex)
        if (next < 0 || next >= count) next = 0
        else {
            next += Number(delta)
            if (next < 0) next = count - 1
            else if (next >= count) next = 0
        }
        root.contextMenuActionIndex = next
    }

    function triggerContextMenuAction(slot) {
        if (root.contextMenuIndex < 0) {
            return
        }
        var action = contextMenuActionForSlot(slot)
        if (action.length <= 0) {
            return
        }
        root.resultContextAction(root.contextMenuIndex, action)
        resultContextMenu.closeMenu()
    }

    Keys.onPressed: function(event) {
        if (!active) {
            return
        }
        if (event.key === Qt.Key_Escape) {
            if (resultContextMenu.visible) {
                closeContextMenu()
                event.accepted = true
                return
            }
            dismissRequested()
            event.accepted = true
            return
        }
        if (resultContextMenu.visible) {
            if (event.key === Qt.Key_Up) {
                moveContextMenuSelection(-1)
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Down) {
                moveContextMenuSelection(1)
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                triggerContextMenuAction(root.contextMenuActionIndex)
                event.accepted = true
                return
            }
        }
        if (event.key === Qt.Key_Up) {
            moveSelection(-1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Tab
                && !(event.modifiers & Qt.ControlModifier)
                && !(event.modifiers & Qt.AltModifier)
                && !(event.modifiers & Qt.MetaModifier)) {
            moveSelectionByGroup((event.modifiers & Qt.ShiftModifier) ? -1 : 1)
            event.accepted = true
            return
        }
        if ((event.modifiers & Qt.ControlModifier)
                && !(event.modifiers & Qt.AltModifier)
                && !(event.modifiers & Qt.MetaModifier)) {
            var quickIdx = -1
            if (event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
                quickIdx = Number(event.key - Qt.Key_1)
            }
            var total = resultsModel ? Number(resultsModel.count || 0) : 0
            if (quickIdx >= 0 && quickIdx < total) {
                root.selectedIndex = quickIdx
                listView.positionViewAtIndex(root.selectedIndex, ListView.Contain)
                activateCurrent()
                event.accepted = true
                return
            }
        }
        if (event.key === Qt.Key_Down) {
            moveSelection(1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if ((event.modifiers & Qt.ControlModifier) && root.selectedIndex >= 0) {
                root.resultContextAction(root.selectedIndex, "openContainingFolder")
                event.accepted = true
                return
            }
            if ((event.modifiers & Qt.AltModifier) && root.selectedIndex >= 0) {
                root.resultContextAction(root.selectedIndex, "properties")
                event.accepted = true
                return
            }
            activateCurrent()
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_C
                && (event.modifiers & Qt.ControlModifier)
                && root.selectedIndex >= 0) {
            if (event.modifiers & Qt.ShiftModifier) {
                root.resultContextAction(root.selectedIndex, "copyUri")
                event.accepted = true
                return
            }
            if (event.modifiers & Qt.AltModifier) {
                root.resultContextAction(root.selectedIndex, "copyName")
                event.accepted = true
                return
            }
            root.resultContextAction(root.selectedIndex, "copyPath")
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_L && (event.modifiers & Qt.ControlModifier)) {
            root.focusSearch()
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_K
                && (event.modifiers & Qt.ControlModifier)
                && !(event.modifiers & Qt.AltModifier)
                && !(event.modifiers & Qt.MetaModifier)) {
            root.closeContextMenu()
            queryField.text = ""
            root.queryTextChangedRequest("")
            var totalAfterClear = resultsModel ? Number(resultsModel.count || 0) : 0
            root.selectedIndex = totalAfterClear > 0 ? 0 : -1
            if (root.selectedIndex >= 0) {
                listView.positionViewAtIndex(root.selectedIndex, ListView.Beginning)
            }
            root.focusSearch()
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Menu
                || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
            if (root.selectedIndex >= 0) {
                listView.positionViewAtIndex(root.selectedIndex, ListView.Contain)
                var px = panel.x + 24
                var py = panel.y + 108
                if (listView.itemAtIndex) {
                    var selectedItem = listView.itemAtIndex(root.selectedIndex)
                    if (selectedItem) {
                        var ix = Math.max(14, selectedItem.width * 0.2)
                        var iy = Math.max(12, selectedItem.height * 0.5)
                        if (selectedItem.mapToGlobal) {
                            var gp = selectedItem.mapToGlobal(ix, iy)
                            openContextMenuForIndexGlobal(root.selectedIndex, gp.x, gp.y)
                            event.accepted = true
                            return
                        }
                        var localPoint = selectedItem.mapToItem(root, ix, iy)
                        px = localPoint.x
                        py = localPoint.y
                    }
                }
                px = Math.max(8, Math.min(root.width - 200, px))
                py = Math.max(8, Math.min(root.height - 120, py))
                openContextMenuForIndex(root.selectedIndex, px, py)
            }
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.compactWindow
        color: Theme.color("overlay")
    }

    Rectangle {
        id: panel
        anchors.horizontalCenter: root.compactWindow ? undefined : parent.horizontalCenter
        anchors.top: root.compactWindow ? undefined : parent.top
        anchors.topMargin: root.compactWindow ? 0 : 64
        anchors.fill: root.compactWindow ? parent : undefined
        width: root.compactWindow ? parent.width : Math.min(parent.width - 48, 760)
        height: root.compactWindow ? parent.height : Math.min(parent.height - 110, 540)
        radius: Theme.radiusXxxl
        color: Theme.color("tothespotPanelBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("tothespotPanelBorder")
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                height: 42
                radius: Theme.radiusWindow
                color: Theme.color("tothespotQueryBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("tothespotQueryBorder")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8
                    Label {
                        text: "Tothespot"
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("smallPlus")
                    }
                    TextField {
                        id: queryField
                        Layout.fillWidth: true
                        background: null
                        placeholderText: "Search apps, files, folders..."
                        text: root.queryText
                        onTextChanged: {
                            root.closeContextMenu()
                            root.queryTextChangedRequest(text)
                        }
                        onAccepted: root.activateCurrent()
                        selectByMouse: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Label {
                    text: (resultsModel ? Number(resultsModel.count || 0) : 0) + " results"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.showDebugInfo
                spacing: 8
                Item {
                    Layout.fillWidth: true
                    implicitHeight: debugSummaryLabel.implicitHeight

                    Label {
                        id: debugSummaryLabel
                        anchors.fill: parent
                        text: "Profile: "
                              + String((root.searchProfileMeta && root.searchProfileMeta.profileId)
                                       ? root.searchProfileMeta.profileId : "balanced")
                              + " | Updated: "
                              + String((root.searchProfileMeta && root.searchProfileMeta.updatedAtUtc)
                                       ? root.searchProfileMeta.updatedAtUtc : "-")
                              + " | Cap: "
                              + String((root.telemetryMeta && root.telemetryMeta.activationCapacity)
                                       ? root.telemetryMeta.activationCapacity : "-")
                              + " | Last: "
                              + String((root.telemetryLast && root.telemetryLast.provider)
                                       ? root.telemetryLast.provider : "-")
                              + " / "
                              + String((root.telemetryLast && root.telemetryLast.action)
                                       ? root.telemetryLast.action : "-")
                              + " / "
                              + String((root.telemetryLast && root.telemetryLast.query)
                                       ? root.telemetryLast.query : "-")
                              + " | Providers: "
                              + String(root.formatProviderStats(root.providerStats))
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("xs")
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: debugSummaryHover
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        hoverEnabled: true
                    }

                    ToolTip.visible: debugSummaryHover.containsMouse
                    ToolTip.delay: 350
                    ToolTip.text: "Profile: "
                                   + String((root.searchProfileMeta && root.searchProfileMeta.profileId)
                                            ? root.searchProfileMeta.profileId : "balanced")
                                   + "\nUpdated: "
                                   + String((root.searchProfileMeta && root.searchProfileMeta.updatedAtUtc)
                                            ? root.searchProfileMeta.updatedAtUtc : "-")
                                   + "\nTelemetry cap: "
                                   + String((root.telemetryMeta && root.telemetryMeta.activationCapacity)
                                            ? root.telemetryMeta.activationCapacity : "-")
                                   + "\nLast provider: "
                                   + String((root.telemetryLast && root.telemetryLast.provider)
                                            ? root.telemetryLast.provider : "-")
                                   + "\nLast action: "
                                   + String((root.telemetryLast && root.telemetryLast.action)
                                            ? root.telemetryLast.action : "-")
                                   + "\nLast query: "
                                   + String((root.telemetryLast && root.telemetryLast.query)
                                            ? root.telemetryLast.query : "-")
                                   + "\nProvider counts: "
                                   + String(root.formatProviderStats(root.providerStats))
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusWindow
                color: Theme.color("tothespotResultsBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("tothespotResultsBorder")
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: listView
                            anchors.fill: parent
                            clip: true
                            spacing: 4
                            model: root.resultsModel
                            boundsBehavior: Flickable.StopAtBounds
                            reuseItems: true
                            cacheBuffer: 480
                            onContentYChanged: root.closeContextMenu()
                            section.property: "sectionTitle"
                            section.criteria: ViewSection.FullString
                            section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart
                            section.delegate: Rectangle {
                                required property string section
                                width: listView.width
                                height: 26
                                color: Theme.color("tothespotSectionBg")
                                Label {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 8
                                    text: section
                                    color: Theme.color("textSecondary")
                                    font.pixelSize: Theme.fontSize("small")
                                    font.weight: Theme.fontWeight("bold")
                                }
                            }

                            delegate: Rectangle {
                        required property int index
                        required property string name
                        required property string path
                        required property string iconName
                        required property bool isDir
                        required property bool isApp
                        required property bool isAction
                        required property string resultKind
                        required property string type
                        required property string provider
                        required property string sectionTitle
                        required property string desktopId
                        required property string executable
                        required property string iconSource
                        property bool appFlag: !!isApp
                        property bool actionFlag: !!isAction
                        property string resultKindRole: String(resultKind || "")
                        property string entryType: String(type || "")
                        property string entryProvider: String(provider || "")
                        property string entrySection: String(sectionTitle || "")
                        property string appDesktopId: String(desktopId || "")
                        property string appExecutable: String(executable || "")
                        property string appIconSource: String(iconSource || "")
                        readonly property bool isActionEntry: actionFlag
                                                             || resultKindRole === "action"
                                                             || entryType === "action"
                                                             || entryProvider === "slm_actions"
                                                             || entrySection === "Actions"
                        readonly property bool isAppEntry: entryType === "app"
                                                          || appFlag
                                                          || entryProvider === "apps"
                                                          || appDesktopId.length > 0
                                                          || appExecutable.length > 0
                                                          || (String(path || "").toLowerCase().indexOf(".desktop") >= 0)
                        readonly property bool isSettingsEntry: entryProvider === "settings"
                                                              || resultKindRole === "settings"
                                                              || entryType === "settings"
                                                              || entryType === "module"
                                                              || entryType === "setting"
                        readonly property bool active: index === root.selectedIndex
                        width: listView.width
                        height: 48
                        radius: Theme.radiusControlLarge
                        color: active
                               ? Theme.color("tothespotRowActive")
                               : (rowMouse.containsMouse ? Theme.color("tothespotRowHover")
                                                         : "transparent")

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10
                            Image {
                                id: iconView
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                fillMode: Image.PreserveAspectFit
                                source: {
                                    if (appIconSource.length > 0) {
                                        if (appIconSource.indexOf("://") >= 0) {
                                            return appIconSource
                                        }
                                        if (appIconSource.charAt(0) === "/") {
                                            return "file://" + appIconSource
                                        }
                                    }
                                    var fallback = "text-x-generic-symbolic"
                                    if (isAppEntry)
                                        fallback = "application-x-executable-symbolic"
                                    else if (isActionEntry)
                                        fallback = "system-run-symbolic"
                                    else if (isSettingsEntry)
                                        fallback = "preferences-system-symbolic"
                                    else if (isDir)
                                        fallback = "folder"
                                    var themed = (iconName && iconName.length > 0) ? iconName : fallback
                                    return "image://themeicon/" + themed
                                            + "?v="
                                            + ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                               ? ThemeIconController.revision : 0)
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: root.highlightQueryText(name, root.queryText)
                                textFormat: Text.StyledText
                                color: Theme.color("textPrimary")
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                visible: !isAppEntry
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: badgeLabel.implicitWidth + 14
                                radius: Theme.radiusControlLarge
                                color: active
                                       ? Theme.color("tothespotBadgeActive")
                                       : Theme.color("tothespotBadgeBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color("tothespotBadgeBorder")
                                Label {
                                    id: badgeLabel
                                    anchors.centerIn: parent
                                    text: {
                                        if (entrySection === "Actions")
                                            return "Action"
                                        if (resultKindRole === "action")
                                            return "Action"
                                        if (resultKindRole === "folder")
                                            return "Folder"
                                        if (resultKindRole === "file")
                                            return "File"
                                        if (resultKindRole === "app")
                                            return "App"
                                        if (resultKindRole === "settings")
                                            return "Setting"
                                        if (isAppEntry)
                                            return "App"
                                        if (isActionEntry)
                                            return "Action"
                                        if (isSettingsEntry)
                                            return "Setting"
                                        if (isDir)
                                            return "Folder"
                                        return "File"
                                    }
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textSecondary")
                                }
                            }
                            Label {
                                visible: !isAppEntry
                                Layout.preferredWidth: Math.min(320, Math.max(120, implicitWidth))
                                text: path
                                color: Theme.color("textSecondary")
                                elide: Text.ElideMiddle
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.LeftButton) {
                                    root.selectedIndex = index
                                }
                            }
                            onDoubleClicked: {
                                root.selectedIndex = index
                                root.resultActivated(index)
                            }
                            onPressed: function(mouse) {
                                if (mouse.button !== Qt.RightButton) {
                                    return
                                }
                                if (rowMouse.mapToGlobal) {
                                    var gp = rowMouse.mapToGlobal(mouse.x, mouse.y)
                                    openContextMenuForIndexGlobal(index, gp.x, gp.y)
                                } else {
                                    var p = rowMouse.mapToItem(root, mouse.x, mouse.y)
                                    openContextMenuForIndex(index, p.x, p.y)
                                }
                                mouse.accepted = true
                            }
                        }
                            }

                            onCountChanged: {
                                root.closeContextMenu()
                                if (count <= 0) {
                                    root.selectedIndex = -1
                                } else if (root.selectedIndex < 0) {
                                    root.selectedIndex = 0
                                } else if (root.selectedIndex >= count) {
                                    root.selectedIndex = count - 1
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: root.compactWindow ? 220 : 180
                        Layout.fillHeight: true
                        visible: root.compactWindow
                        radius: Theme.radiusCard
                        color: Theme.color("tothespotPreviewBg")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("tothespotPreviewBorder")

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            Label {
                                text: "Preview"
                                font.pixelSize: Theme.fontSize("small")
                                font.weight: Theme.fontWeight("bold")
                                color: Theme.color("textSecondary")
                            }

                            Label {
                                width: parent.width
                                text: String((root.previewData && root.previewData.name) ? root.previewData.name : "-")
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: Theme.lineHeight("tight")
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("body")
                                font.weight: Theme.fontWeight("bold")
                            }

                            Label {
                                width: parent.width
                                text: {
                                    if (!root.previewData || !root.previewData.kind) return ""
                                    if (String(root.previewData.kind) === "app") return "Application"
                                    if (String(root.previewData.kind) === "action") return "Action"
                                    if (String(root.previewData.kind) === "settings") return "Setting"
                                    return !!root.previewData.isDir ? "Folder" : "File"
                                }
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }

                            Label {
                                width: parent.width
                                visible: !!(root.previewData
                                            && String(root.previewData.kind) === "action"
                                            && String(root.previewData.intent || "").length > 0)
                                text: "Intent: " + String(root.previewData ? root.previewData.intent : "")
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: Theme.lineHeight("normal")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }

                            Label {
                                width: parent.width
                                visible: !!(root.previewData
                                            && String(root.previewData.kind) === "action"
                                            && (String(root.previewData.desktopId || "").length > 0
                                                || String(root.previewData.desktopFile || "").length > 0))
                                text: {
                                    if (!root.previewData) return ""
                                    var src = String(root.previewData.desktopId || "")
                                    if (src.length <= 0) {
                                        src = String(root.previewData.desktopFile || "")
                                    }
                                    return "Source: " + src
                                }
                                wrapMode: Text.WrapAnywhere
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: Theme.lineHeight("normal")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }

                            Label {
                                width: parent.width
                                visible: !!(root.previewData && root.previewData.mimeType)
                                text: "MIME: " + String(root.previewData ? root.previewData.mimeType : "")
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: Theme.lineHeight("normal")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }

                            Label {
                                width: parent.width
                                visible: !!(root.previewData && root.previewData.kind === "path" && !root.previewData.isDir)
                                text: "Size: " + formatBytes(root.previewData ? root.previewData.size : 0)
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }

                            Label {
                                width: parent.width
                                visible: !!(root.previewData && root.previewData.modified)
                                text: "Modified: " + String(root.previewData ? root.previewData.modified : "")
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: Theme.lineHeight("normal")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }

                            Label {
                                width: parent.width
                                visible: !!(root.previewData && root.previewData.path)
                                text: String(root.previewData ? root.previewData.path : "")
                                wrapMode: Text.WrapAnywhere
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: Theme.lineHeight("normal")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("xs")
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        id: resultContextMenu
        visible: false
        z: 2000
        width: 220
        height: contextMenuColumn.implicitHeight + 12

        function openAt(xPos, yPos) {
            if (contextMenuColumn.forceLayout) {
                contextMenuColumn.forceLayout()
            }
            var menuW = Math.max(220, Math.ceil(width))
            var menuH = Math.max(32, Math.ceil(contextMenuColumn.implicitHeight + 12))
            width = menuW
            height = menuH
            x = Math.max(8, Math.min(root.width - menuW - 8, Math.round(xPos)))
            y = Math.max(8, Math.min(root.height - menuH - 8, Math.round(yPos)))
            root.contextMenuOpenedAtMs = Date.now()
            visible = true
        }

        function closeMenu() {
            visible = false
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusControlLarge
            color: Theme.color("menuBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("menuBorder")
        }

        Column {
            id: contextMenuColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            anchors.topMargin: 6
            spacing: 2

            Repeater {
                model: root.contextMenuIndex >= 0 ? root.contextMenuVisibleActionCount() : 0
                delegate: Rectangle {
                    required property int index
                    readonly property string actionId: root.contextMenuActionForSlot(index)
                    width: contextMenuColumn.width
                    height: 32
                    radius: Theme.radiusMd
                    color: (contextMenuItemMouse.containsMouse || root.contextMenuActionIndex === index)
                           ? Theme.color("accentSoft") : "transparent"

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        text: root.contextMenuLabelForAction(parent.actionId)
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("smallPlus")
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        text: root.contextMenuShortcutForAction(parent.actionId)
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        visible: text.length > 0
                    }

                    MouseArea {
                        id: contextMenuItemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: root.contextMenuActionIndex = parent.index
                        onClicked: triggerContextMenuAction(parent.index)
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1999
        visible: resultContextMenu.visible
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        onPressed: function(mouse) {
            if ((Date.now() - root.contextMenuOpenedAtMs) < 120) {
                mouse.accepted = true
                return
            }
            var inside = mouse.x >= resultContextMenu.x
                         && mouse.x <= resultContextMenu.x + resultContextMenu.width
                         && mouse.y >= resultContextMenu.y
                         && mouse.y <= resultContextMenu.y + resultContextMenu.height
            if (!inside) {
                closeContextMenu()
                mouse.accepted = true
            }
        }
    }
}
