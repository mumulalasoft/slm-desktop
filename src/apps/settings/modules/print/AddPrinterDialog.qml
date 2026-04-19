import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle
import SlmStyle as DSStyle
import "components" as AddPrinterUi

Popup {
    id: root

    // ── Public interface ───────────────────────────────────────────────
    property var    printerAdmin:      null
    property bool   hasBlockingIssues: false
    property string errorText:         ""

    signal printerAdded()

    // ── State machine ──────────────────────────────────────────────────
    readonly property int stateInit:          0
    readonly property int stateScanning:      1
    readonly property int statePartialResult: 2
    readonly property int stateResultReady:   3
    readonly property int stateEmpty:         4
    readonly property int stateError:         5
    readonly property int stateSelected:      6

    // ── Internal state ─────────────────────────────────────────────────
    property int    _mode:             0   // 0=Local  1=IP  2=Browse
    property int    _uiState:          stateInit
    property var    _allDevices:       []
    property var    _displayDevices:   []
    property var    _pendingReveal:    []
    property string _selectedUri:      ""
    property int    _pendingFillIdx:   -1
    property int    _selectedIdx:      -1
    property string _search:           ""
    property string _name:             ""
    property string _location:         ""
    property bool   _adding:           false
    property bool   _scanning:         false
    property bool   _scanTimedOut:     false
    property bool   _scanCompleted:    false
    property bool   _showTimeHint:     false
    property bool   _updatingNetAddr:  false
    property string _scanStatusText:   ""
    property string _scanStatusVisualText: ""
    property string _nextScanStatusText: ""
    property real   _scanMessageOpacity: 1.0
    property int    _scanMsgIndex:     0

    readonly property var _tabLabels: [qsTr("Local"), qsTr("IP"), qsTr("Browse")]
    readonly property var _tabIcons:  ["🖨",             "🌐",          "🧩"]

    readonly property var _localScanMessages: [
        qsTr("Searching for printers…"),
        qsTr("Checking USB devices…"),
        qsTr("Looking for driverless printers…")
    ]

    readonly property var _browseScanMessages: [
        qsTr("Searching for printers…"),
        qsTr("Discovering network printers…"),
        qsTr("Looking for nearby printers…"),
        qsTr("Almost done…")
    ]

    // ── Computed lists ────────────────────────────────────────────────
    readonly property var _localDevices: {
        var q = _search.trim().toLowerCase()
        return (_allDevices || []).filter(function(d) {
            return String(d.uri || "").indexOf("usb://") === 0
                || String(d.uri || "").indexOf("ipp-usb://") === 0
        }).filter(function(d) {
            return !q || _label(d).toLowerCase().indexOf(q) >= 0
        })
    }

    readonly property var _browseDevices: {
        var q = _search.trim().toLowerCase()
        return (_allDevices || []).filter(function(d) {
            var u = String(d.uri || "")
            return (u.indexOf("dnssd://") === 0
                    || u.indexOf("ipp://") === 0
                    || u.indexOf("ipps://") === 0
                    || u.indexOf("lpd://") === 0
                    || u.indexOf("socket://") === 0)
                   && u.indexOf("localhost") < 0
                   && u.indexOf("127.0.0.1") < 0
                   && u.indexOf("[::1]") < 0
        }).filter(function(d) {
            return !q || _label(d).toLowerCase().indexOf(q) >= 0
        })
    }

    readonly property var _devices: _mode === 2 ? _browseDevices : _localDevices

    readonly property bool _canAdd: {
        if (hasBlockingIssues || _adding) return false
        if (_mode === 1)
            return netAddrField.text.trim().length > 0 && _name.trim().length > 0
        return _selectedIdx >= 0 && _selectedIdx < _displayDevices.length && _name.trim().length > 0
    }

    // ── Helpers ────────────────────────────────────────────────────────
    function _label(d) {
        var info = String(d && d.info || "").trim()
        if (info)
            return info

        var u = String(d && d.uri || "")
        var m = u.match(/^usb:\/\/([^/]+)\/([^/?]+)/)
        if (m)
            return decodeURIComponent(m[1]) + " " + decodeURIComponent(m[2])

        return u.replace(/^[a-z\-]+:\/\//, "").split("/")[0].split(":")[0] || u
    }

    function _kind(d) {
        var u = String(d && d.uri || "")
        if (u.indexOf("usb://") === 0 || u.indexOf("ipp-usb://") === 0)
            return qsTr("USB")
        return qsTr("Network")
    }

    function _scanTimeoutMs() {
        return _mode === 2 ? 7000 : 3200
    }

    function _refreshIntervalMs() {
        return _displayDevices.length > 0 ? 16000 : 9000
    }

    function _scanMessages() {
        return _mode === 2 ? _browseScanMessages : _localScanMessages
    }

    function _setScanStatusByIndex(idx) {
        var msgs = _scanMessages()
        var nextText = qsTr("Searching for printers…")
        if (!msgs || msgs.length === 0) {
            _scanStatusText = nextText
            _scanStatusVisualText = nextText
            return
        }
        var safe = ((idx % msgs.length) + msgs.length) % msgs.length
        nextText = msgs[safe]
        _scanStatusText = nextText

        if (_scanStatusVisualText.length === 0) {
            _scanStatusVisualText = nextText
            _scanMessageOpacity = 1.0
            return
        }

        if (_scanStatusVisualText === nextText)
            return

        _nextScanStatusText = nextText
        scanMessageSwap.restart()
    }

    function _syncSelectionByUri() {
        _selectedIdx = -1
        if (!_selectedUri || _selectedUri.length === 0)
            return

        for (var i = 0; i < _displayDevices.length; ++i) {
            if (String(_displayDevices[i].uri || "") === _selectedUri) {
                _selectedIdx = i
                return
            }
        }
    }

    function _applySelectionFieldsFromCurrent() {
        if (_selectedIdx < 0 || _selectedIdx >= _displayDevices.length)
            return

        var d = _displayDevices[_selectedIdx]
        var lbl = _label(d)
        var safe = lbl.replace(/[^A-Za-z0-9_\-]/g, "_")
                      .replace(/_+/g, "_")
                      .replace(/^_|_$/g, "")
                      .slice(0, 64) || "Printer"

        _name = safe
        nameField.text = safe

        var u = String(d.uri || "")
        var loc = (u.indexOf("usb://") === 0 || u.indexOf("ipp-usb://") === 0)
                  ? qsTr("USB")
                  : u.replace(/^[a-z\-]+:\/\//, "").split("/")[0].split(":")[0]

        _location = loc
        locationField.text = loc
        driverBox.currentIndex = 0
    }

    function _selectDevice(idx) {
        if (idx < 0 || idx >= _displayDevices.length)
            return

        _selectedIdx = idx
        _pendingFillIdx = idx
        _selectedUri = String(_displayDevices[idx].uri || "")
        errorText = ""
        selectionCommitTimer.restart()
        _syncUiState()
    }

    function _uri() {
        if (_mode === 1) {
            var a = netAddrField.text.trim()
            var p = protocolBox.currentIndex
            if (p === 1)
                return "ipps://" + a + "/ipp/print"
            if (p === 2)
                return "socket://" + a + ":9100"
            return "ipp://" + a + "/ipp/print"
        }

        return (_selectedIdx >= 0 && _selectedIdx < _displayDevices.length)
               ? String(_displayDevices[_selectedIdx].uri || "") : ""
    }

    function _doAdd() {
        if (!_canAdd || !root.printerAdmin)
            return

        var ppd = driverBox.currentIndex === 0 ? "everywhere" : ""
        errorText = ""
        _adding = true
        root.printerAdmin.addPrinter(_name.trim(), _uri(), ppd)
    }

    function _syncUiState() {
        if (hasBlockingIssues || !printerAdmin) {
            _uiState = stateError
            return
        }

        if (_mode === 1) {
            _uiState = (_name.trim().length > 0 && netAddrField.text.trim().length > 0)
                       ? stateSelected : stateResultReady
            return
        }

        if (_scanning) {
            _uiState = _displayDevices.length > 0 ? statePartialResult : stateScanning
            return
        }

        if (_displayDevices.length === 0 && _scanTimedOut) {
            _uiState = stateEmpty
            return
        }

        if (_selectedIdx >= 0) {
            _uiState = stateSelected
            return
        }

        _uiState = _displayDevices.length > 0 ? stateResultReady : stateScanning
    }

    function _refreshVisibleDevices(progressive) {
        var target = _devices
        var targetUris = {}
        for (var i = 0; i < target.length; ++i)
            targetUris[String(target[i].uri || "")] = true

        var kept = []
        for (var j = 0; j < _displayDevices.length; ++j) {
            var shown = _displayDevices[j]
            if (targetUris[String(shown.uri || "")])
                kept.push(shown)
        }

        if (!progressive) {
            _pendingReveal = []
            revealTimer.stop()
            _displayDevices = target
            _syncSelectionByUri()
            _syncUiState()
            return
        }

        _displayDevices = kept

        var shownUris = {}
        for (var k = 0; k < _displayDevices.length; ++k)
            shownUris[String(_displayDevices[k].uri || "")] = true

        _pendingReveal = []
        for (var n = 0; n < target.length; ++n) {
            var candidate = target[n]
            var uri = String(candidate.uri || "")
            if (!shownUris[uri])
                _pendingReveal.push(candidate)
        }

        if (_pendingReveal.length > 0)
            revealTimer.start()
        else
            revealTimer.stop()

        _syncSelectionByUri()
        _syncUiState()
    }

    function _autoSelectSingleDevice() {
        if (_mode === 1 || _selectedIdx >= 0)
            return

        if (_displayDevices.length === 1)
            _selectDevice(0)
    }

    function _beginScan() {
        if (_mode === 1)
            return

        if (hasBlockingIssues || !printerAdmin) {
            _scanning = false
            _uiState = stateError
            errorText = hasBlockingIssues
                    ? qsTr("Unable to search for printers")
                    : qsTr("Printing service is unavailable")
            return
        }

        if (printerAdmin.busy)
            return

        _scanTimedOut = false
        _scanCompleted = false
        _showTimeHint = false
        _scanning = true
        _scanMsgIndex = 0
        _setScanStatusByIndex(_scanMsgIndex)
        scanStatusTimer.start()
        scanTimeoutTimer.interval = _scanTimeoutMs()
        scanTimeoutTimer.restart()
        scanHintTimer.restart()

        root.printerAdmin.discoverDevices()
        _syncUiState()
    }

    function _stopScanIndicators() {
        _scanning = false
        scanStatusTimer.stop()
        scanTimeoutTimer.stop()
        scanHintTimer.stop()
        _showTimeHint = false
    }

    function _switchMode(m) {
        _mode = m
        _search = ""
        _selectedIdx = -1
        _selectedUri = ""
        _pendingFillIdx = -1
        searchField.text = ""
        errorText = ""

        _pendingReveal = []
        revealTimer.stop()
        selectionCommitTimer.stop()

        if (_mode === 1) {
            _stopScanIndicators()
            _refreshVisibleDevices(false)
            _syncUiState()
            return
        }

        _refreshVisibleDevices(false)
        _beginScan()
    }

    function _retryScan() {
        errorText = ""
        if (_mode !== 1)
            _beginScan()
    }

    function _goToIpMode() {
        _switchMode(1)
    }

    function _autodetectProtocol(rawText) {
        var value = String(rawText || "").trim().toLowerCase()
        if (value.indexOf("ipps://") === 0)
            return 1
        if (value.indexOf("socket://") === 0 || value.indexOf(":9100") > 0)
            return 2
        if (value.indexOf("ipp://") === 0 || value.indexOf(":631") > 0)
            return 0
        return protocolBox.currentIndex
    }

    function _normalizeNetworkAddress(rawText) {
        var value = String(rawText || "").trim()
        value = value.replace(/^ipps?:\/\//i, "")
        value = value.replace(/^socket:\/\//i, "")
        value = value.replace(/\/ipp\/print$/i, "")
        value = value.replace(/:\d+$/i, "")
        return value
    }

    function _reset() {
        _mode = 0
        _uiState = stateInit
        _allDevices = []
        _displayDevices = []
        _pendingReveal = []
        _selectedUri = ""
        _selectedIdx = -1
        _search = ""
        _name = ""
        _location = ""
        _adding = false
        _scanning = false
        _scanTimedOut = false
        _scanCompleted = false
        _showTimeHint = false
        _scanStatusText = qsTr("Searching for printers…")
        _scanStatusVisualText = _scanStatusText
        _nextScanStatusText = ""
        _scanMessageOpacity = 1.0
        _scanMsgIndex = 0
        _updatingNetAddr = false
        errorText = ""

        searchField.text = ""
        nameField.text = ""
        locationField.text = ""
        netAddrField.text = ""
        protocolBox.currentIndex = 0
        driverBox.currentIndex = 0

        refreshTimer.interval = _refreshIntervalMs()
        refreshTimer.stop()
        revealTimer.stop()
        scanStatusTimer.stop()
        scanTimeoutTimer.stop()
        scanHintTimer.stop()
        scanMessageSwap.stop()
        selectionCommitTimer.stop()
    }

    Timer {
        id: selectionCommitTimer
        interval: 70
        repeat: false
        onTriggered: {
            if (root._pendingFillIdx >= 0 && root._pendingFillIdx < root._displayDevices.length) {
                root._selectedIdx = root._pendingFillIdx
                root._applySelectionFieldsFromCurrent()
            }
            root._pendingFillIdx = -1
        }
    }

    Timer {
        id: scanStatusTimer
        interval: 1400
        repeat: true
        onTriggered: {
            root._scanMsgIndex += 1
            root._setScanStatusByIndex(root._scanMsgIndex)
        }
    }

    SequentialAnimation {
        id: scanMessageSwap
        running: false
        NumberAnimation {
            target: root
            property: "_scanMessageOpacity"
            to: 0.0
            duration: 90
            easing.type: Easing.OutQuad
        }
        ScriptAction {
            script: root._scanStatusVisualText = root._nextScanStatusText
        }
        NumberAnimation {
            target: root
            property: "_scanMessageOpacity"
            to: 1.0
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    Timer {
        id: scanTimeoutTimer
        interval: 3200
        repeat: false
        onTriggered: {
            if (root._displayDevices.length === 0 && root._mode !== 1) {
                root._scanTimedOut = true
                root._stopScanIndicators()
                root._syncUiState()
            }
        }
    }

    Timer {
        id: scanHintTimer
        interval: 5000
        repeat: false
        onTriggered: {
            root._showTimeHint = true
        }
    }

    Timer {
        id: revealTimer
        interval: 170
        repeat: true
        onTriggered: {
            if (root._pendingReveal.length === 0) {
                stop()
                root._autoSelectSingleDevice()
                root._syncUiState()
                return
            }

            var next = root._pendingReveal.shift()
            root._displayDevices = root._displayDevices.concat([next])
            root._syncSelectionByUri()
            root._autoSelectSingleDevice()
            root._syncUiState()
        }
    }

    Timer {
        id: refreshTimer
        interval: 9000
        repeat: true
        running: root.opened
        onTriggered: {
            if (root._mode !== 1)
                root._beginScan()
        }
    }

    // ── Backend connections ────────────────────────────────────────────
    Connections {
        target: root.printerAdmin

        function onDiscoveredDevicesChanged() {
            root._allDevices = root.printerAdmin ? root.printerAdmin.discoveredDevices : []

            // During busy=true phase, this can switch from empty->non-empty.
            // Render progressively so the list feels alive.
            root._refreshVisibleDevices(true)

            if (root._displayDevices.length > 0) {
                root._scanTimedOut = false
                root.refreshTimer.interval = root._refreshIntervalMs()
                root.refreshTimer.restart()
            }

            root._syncUiState()
        }

        function onBusyChanged() {
            if (!root.printerAdmin)
                return

            if (root.printerAdmin.busy) {
                root._scanning = true
                root._syncUiState()
                return
            }

            root._scanCompleted = true

            // Keep scanning feedback alive until timeout only when no devices yet.
            if (root._displayDevices.length > 0) {
                root._stopScanIndicators()
                root._scanTimedOut = false
            }

            root._autoSelectSingleDevice()
            root._syncUiState()
        }

        function onPrinterAdded(success, name, error) {
            root._adding = false
            if (success) {
                root.printerAdded()
                root.close()
                return
            }

            root.errorText = (error && error.length > 0)
                    ? error
                    : qsTr("Could not add the printer. Check the address and try again.")
        }
    }

    // ── Popup config ───────────────────────────────────────────────────
    modal:       true
    focus:       true
    closePolicy: Popup.CloseOnEscape
    width:       560
    height:      620
    padding:     0
    parent:      Overlay.overlay
    x:           parent ? Math.round((parent.width - width) * 0.5) : 0
    y:           parent ? Math.round((parent.height - height) * 0.5) : 0

    onOpened: {
        _reset()
        _uiState = stateInit
        _beginScan()
        refreshTimer.interval = _refreshIntervalMs()
        refreshTimer.start()
    }

    onClosed: {
        _stopScanIndicators()
        refreshTimer.stop()
        revealTimer.stop()
        selectionCommitTimer.stop()
    }

    // ── Background ─────────────────────────────────────────────────────
    background: Rectangle {
        radius: Theme.radiusWindowAlt
        color:  Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("windowCardBorder")
    }

    // ── Content ────────────────────────────────────────────────────────
    contentItem: ColumnLayout {
        spacing: 0

        // ── Header ──────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth:    true
            Layout.leftMargin:   8
            Layout.rightMargin:  8
            Layout.topMargin:    6
            Layout.bottomMargin: 6
            spacing: 0

            DSStyle.Button {
                flat: true
                text: qsTr("Cancel")
                font.pixelSize: Theme.fontSize("body")
                onClicked: root.close()
            }

            DSStyle.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Add Printer")
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("semibold")
                color: Theme.color("textPrimary")
                elide: Text.ElideRight
            }

            DSStyle.Button {
                id: addButton
                flat: true
                text: root._adding ? qsTr("Adding…") : qsTr("Add")
                font.pixelSize: Theme.fontSize("body")
                enabled: root._canAdd
                opacity: enabled ? 1.0 : 0.56
                Behavior on opacity {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }
                onClicked: root._doAdd()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: Theme.borderWidthThin
            color: Theme.color("panelBorder")
        }

        // ── Tabs ────────────────────────────────────────────────────────
        AddPrinterUi.AnimatedTabSwitcher {
            Layout.fillWidth:    true
            Layout.leftMargin:   12
            Layout.rightMargin:  12
            Layout.topMargin:    10
            Layout.bottomMargin: 10
            currentIndex: root._mode
            labels: root._tabLabels
            icons: root._tabIcons
            onTabSelected: function(index) { root._switchMode(index) }
        }

        // ── Search (Local & Browse) ────────────────────────────────────
        DSStyle.TextField {
            id: searchField
            Layout.fillWidth:    true
            Layout.leftMargin:   12
            Layout.rightMargin:  12
            Layout.bottomMargin: 8
            visible: root._mode !== 1
            placeholderText: qsTr("Search printers…")
            font.pixelSize: Theme.fontSize("body")
            onTextChanged: {
                root._search = text
                root._refreshVisibleDevices(false)
            }
        }

        // ── Main panel ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth:    true
            Layout.fillHeight:   true
            Layout.leftMargin:   12
            Layout.rightMargin:  12
            Layout.bottomMargin: 10
            radius: Theme.radiusControl
            color:  Theme.color("surface")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")
            clip: true

            // Subtle visual anchor while scanning
            Text {
                visible: root._mode !== 1
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                text: "🖨"
                font.pixelSize: Theme.fontSize("display")
                opacity: (root._uiState === root.stateScanning || root._uiState === root.statePartialResult) ? 0.10 : 0.05

                SequentialAnimation on opacity {
                    running: root._uiState === root.stateScanning || root._uiState === root.statePartialResult
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.16; duration: 1100 }
                    NumberAnimation { to: 0.08; duration: 900 }
                }
            }

            // ── IP mode panel ───────────────────────────────────────────
            AddPrinterUi.StatePanel {
                active: root._mode === 1
                restY: 0

                ColumnLayout {
                    anchors { fill: parent; margins: 16 }
                    spacing: 14

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Address")
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                        }
                        DSStyle.TextField {
                            id: netAddrField
                            Layout.fillWidth: true
                            placeholderText: qsTr("192.168.1.100  or  printer.local")
                            font.pixelSize: Theme.fontSize("body")
                            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                            onTextChanged: {
                                if (root._updatingNetAddr)
                                    return

                                var p = root._autodetectProtocol(text)
                                if (p !== protocolBox.currentIndex)
                                    protocolBox.currentIndex = p

                                var normalized = root._normalizeNetworkAddress(text)
                                if (normalized !== text.trim()) {
                                    root._updatingNetAddr = true
                                    text = normalized
                                    root._updatingNetAddr = false
                                }

                                if (text.trim().length > 0 && root._name.length === 0) {
                                    var h = text.trim().split(".")[0]
                                                .replace(/[^A-Za-z0-9_\-]/g, "_")
                                                .slice(0, 32)
                                    root._name = h || "Network_Printer"
                                    nameField.text = root._name
                                }

                                root._syncUiState()
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Protocol")
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                        }
                        ComboBox {
                            id: protocolBox
                            Layout.preferredWidth: 260
                            model: [
                                qsTr("Standard (recommended)"),
                                qsTr("Secure (encrypted)"),
                                qsTr("JetDirect (port 9100)")
                            ]
                            font.pixelSize: Theme.fontSize("body")
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ── Error state ─────────────────────────────────────────────
            AddPrinterUi.StatePanel {
                active: root._mode !== 1 && root._uiState === root.stateError

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Unable to search for printers")
                        font.pixelSize: Theme.fontSize("body")
                        font.weight: Theme.fontWeight("medium")
                        color: Theme.color("textPrimary")
                    }

                    DSStyle.Button {
                        Layout.alignment: Qt.AlignHCenter
                        flat: true
                        text: qsTr("Retry")
                        onClicked: root._retryScan()
                    }
                }
            }

            // ── Scanning only state ─────────────────────────────────────
            AddPrinterUi.StatePanel {
                active: root._mode !== 1
                        && root._uiState === root.stateScanning
                        && root._displayDevices.length === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10

                    AddPrinterUi.AnimatedSpinner {
                        Layout.alignment: Qt.AlignHCenter
                        active: parent.visible
                        accentColor: Theme.color("accent")
                    }

                    Text {
                        id: scanStatusLabel
                        Layout.alignment: Qt.AlignHCenter
                        text: root._scanStatusVisualText
                        opacity: root._scanMessageOpacity
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textSecondary")
                    }

                    Text {
                        visible: root._showTimeHint
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Still searching. You can add by IP while discovery continues.")
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12

                        DSStyle.Button {
                            flat: true
                            text: qsTr("Add by IP")
                            onClicked: root._goToIpMode()
                        }

                        DSStyle.Button {
                            flat: true
                            text: qsTr("Refresh")
                            onClicked: root._retryScan()
                        }
                    }
                }
            }

            // ── List area (result / partial / selected) ─────────────────
            AddPrinterUi.StatePanel {
                active: root._mode !== 1 && root._displayDevices.length > 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 0
                    spacing: 0

                    Rectangle {
                        visible: root._uiState === root.statePartialResult || root._uiState === root.stateScanning
                        Layout.fillWidth: true
                        implicitHeight: 30
                        color: Theme.color("controlBg")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("panelBorder")

                        Row {
                            anchors.centerIn: parent
                            spacing: 8

                            AddPrinterUi.AnimatedSpinner {
                                active: parent.parent.visible
                                implicitWidth: 16
                                implicitHeight: 16
                                accentColor: Theme.color("textSecondary")
                            }
                            Text {
                                text: qsTr("Scanning continues…")
                                font.pixelSize: Theme.fontSize("small")
                                color: Theme.color("textSecondary")
                            }
                        }
                    }

                    ListView {
                        id: deviceList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root._displayDevices
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: AddPrinterUi.PrinterListItem {
                            required property var modelData
                            required property int index
                            width: deviceList.width
                            selected: root._selectedIdx === index
                            revealDelayMs: Math.min(220, index * 28)
                            title: root._label(modelData)
                            subtitle: root._kind(modelData) + " · " + qsTr("Ready")
                            onSelectedByUser: root._selectDevice(index)
                        }
                    }
                }
            }

            // ── Empty state ─────────────────────────────────────────────
            AddPrinterUi.StatePanel {
                active: root._mode !== 1 && root._uiState === root.stateEmpty

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("No printers available yet")
                        font.pixelSize: Theme.fontSize("body")
                        font.weight: Theme.fontWeight("medium")
                        color: Theme.color("textPrimary")
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Connect a printer or try another method")
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Column {
                        spacing: 2
                        Text {
                            text: qsTr("• Plug in via USB")
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                        }
                        Text {
                            text: qsTr("• Use IP tab for manual setup")
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                        }
                        Text {
                            text: qsTr("• Check network printers in Browse")
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12

                        DSStyle.Button {
                            flat: true
                            text: qsTr("Add by IP")
                            onClicked: root._goToIpMode()
                        }

                        DSStyle.Button {
                            flat: true
                            text: qsTr("Refresh")
                            onClicked: root._retryScan()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: Theme.borderWidthThin
            color: Theme.color("panelBorder")
        }

        // ── Detail panel ────────────────────────────────────────────────
        Rectangle {
            id: detailPanel
            Layout.fillWidth:    true
            Layout.leftMargin:   12
            Layout.rightMargin:  12
            Layout.topMargin:    12
            Layout.bottomMargin: 10
            implicitHeight: detailFields.implicitHeight + 24
            radius: Theme.radiusControl
            color: Theme.color("windowCard")
            border.width: Theme.borderWidthThin
            border.color: root._selectedIdx >= 0 ? Theme.color("accent") : Theme.color("panelBorder")
            Behavior on border.color {
                ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                id: detailFields
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8

                AddPrinterUi.AnimatedDetailField {
                    Layout.fillWidth: true
                    label: qsTr("Name")
                    active: root._selectedIdx >= 0
                    DSStyle.TextField {
                        id: nameField
                        anchors.left: parent.left
                        anchors.right: parent.right
                        placeholderText: qsTr("e.g. Office_Printer")
                        font.pixelSize: Theme.fontSize("body")
                        onTextChanged: {
                            root._name = text
                            root._syncUiState()
                        }
                    }
                }

                AddPrinterUi.AnimatedDetailField {
                    Layout.fillWidth: true
                    label: qsTr("Location")
                    active: root._selectedIdx >= 0
                    DSStyle.TextField {
                        id: locationField
                        anchors.left: parent.left
                        anchors.right: parent.right
                        placeholderText: qsTr("Optional")
                        font.pixelSize: Theme.fontSize("body")
                        onTextChanged: root._location = text
                    }
                }

                AddPrinterUi.AnimatedDetailField {
                    Layout.fillWidth: true
                    label: qsTr("Use")
                    active: root._selectedIdx >= 0
                    ComboBox {
                        id: driverBox
                        anchors.left: parent.left
                        model: [qsTr("Auto (Recommended)"), qsTr("Generic")]
                        font.pixelSize: Theme.fontSize("body")
                        width: 220
                    }
                }
            }
        }

        // ── Error banner (add printer failures) ────────────────────────
        Rectangle {
            visible: root.errorText.length > 0
            Layout.fillWidth:    true
            Layout.leftMargin:   12
            Layout.rightMargin:  12
            Layout.bottomMargin: 8
            radius: Theme.radiusMd
            color: Theme.color("errorSoft")
            implicitHeight: errLabel.implicitHeight + 16

            Text {
                id: errLabel
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: 12
                }
                text: root.errorText
                color: Theme.color("error")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
            }
        }
    }
}
