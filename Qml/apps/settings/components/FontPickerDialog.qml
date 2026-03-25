import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

// Modal font picker dialog.
// Usage:
//   FontPickerDialog {
//       id: picker
//       initialSpec: FontManager.defaultFont
//       onFontPicked: function(spec) { FontManager.setDefaultFont(spec) }
//   }
//   ...
//   picker.open()
Popup {
    id: root

    property string initialSpec: ""
    signal fontPicked(string spec)

    // Internal state ─────────────────────────────────────────
    property string _family: ""
    property string _style:  "Regular"
    property int    _size:   11
    property string _search: ""

    readonly property string _currentSpec:
        (_family.length > 0) ? (_family + "," + _style + "," + _size) : ""

    // ────────────────────────────────────────────────────────
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    width: 440
    height: 540
    padding: 0

    // Attach to application window for proper modal overlay + centering
    parent: Overlay.overlay

    x: parent ? Math.round((parent.width  - width)  * 0.5) : 0
    y: parent ? Math.round((parent.height - height) * 0.5) : 0

    // Font entries list model, populated lazily on open
    ListModel { id: fontModel }

    // Filtered proxy via JS filter pass
    property var _filteredRows: []

    function _rebuildFilter() {
        var q = _search.trim().toLowerCase()
        var result = []
        for (var i = 0; i < fontModel.count; ++i) {
            var item = fontModel.get(i)
            if (q.length === 0 || item.displayName.toLowerCase().indexOf(q) >= 0)
                result.push(i)
        }
        _filteredRows = result
        fontListView.model = _filteredRows.length
    }

    function _open(spec) {
        // Parse initial spec
        var parts = (spec || "").split(",")
        _family = parts[0] ? parts[0].trim() : ""
        _style  = parts[1] ? parts[1].trim() : "Regular"
        _size   = parseInt(parts[2]) || 11
        _search = ""
        searchField.text = ""

        // Build font model if not yet done
        if (fontModel.count === 0) {
            var entries = FontManager.allFontEntries()
            for (var i = 0; i < entries.length; ++i) {
                var e = entries[i]
                fontModel.append({
                    family:      e.family,
                    style:       e.style,
                    weight:      e.weight,
                    isItalic:    e.italic,
                    displayName: e.displayName
                })
            }
        }

        _rebuildFilter()

        // Scroll to current selection
        Qt.callLater(function() {
            for (var j = 0; j < _filteredRows.length; ++j) {
                var idx = _filteredRows[j]
                var row = fontModel.get(idx)
                if (row.family === _family && row.style === _style) {
                    fontListView.positionViewAtIndex(j, ListView.Center)
                    break
                }
            }
        })
    }

    onOpened: _open(initialSpec)

    // ── Background ───────────────────────────────────────────
    background: Rectangle {
        radius: Theme.radiusWindowAlt
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("windowCardBorder")
        layer.enabled: true
        layer.effect: null
    }

    // ── Content ──────────────────────────────────────────────
    contentItem: ColumnLayout {
        spacing: 0

        // ── Header bar ──────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            spacing: 0

            Button {
                flat: true
                text: qsTr("Cancel")
                font.pixelSize: Theme.fontSize("body")
                onClicked: root.close()
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Pick a Font")
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("semibold")
                color: Theme.color("textPrimary")
                elide: Text.ElideRight
            }

            Button {
                flat: true
                text: qsTr("Select")
                font.pixelSize: Theme.fontSize("body")
                enabled: root._family.length > 0
                onClicked: {
                    root.fontPicked(root._currentSpec)
                    root.close()
                }
            }
        }

        // ── Divider ─────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.color("panelBorder")
        }

        // ── Search ──────────────────────────────────────────
        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.margins: 8
            placeholderText: qsTr("Search font name")
            font.pixelSize: Theme.fontSize("body")
            leftPadding: 8
            onTextChanged: {
                root._search = text
                root._rebuildFilter()
            }
        }

        // ── Font list ────────────────────────────────────────
        ListView {
            id: fontListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 4
            clip: true
            model: root._filteredRows.length

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ItemDelegate {
                id: fontDelegate
                required property int index

                readonly property int sourceIdx:    root._filteredRows[index] || 0
                readonly property var sourceRow:    fontModel.get(sourceIdx) || {}
                readonly property string rowFamily: sourceRow.family || ""
                readonly property string rowStyle:  sourceRow.style  || ""
                readonly property bool isSelected:
                    root._family === rowFamily && root._style === rowStyle

                width: fontListView.width
                height: Theme.metric("controlHeightLarge")
                highlighted: isSelected
                padding: 0

                contentItem: Text {
                    leftPadding: 12
                    text: sourceRow.displayName || ""
                    color: isSelected
                        ? Theme.color("accentText")
                        : Theme.color("textPrimary")
                    font.family:  rowFamily
                    font.weight:  sourceRow.weight || 400
                    font.italic:  sourceRow.isItalic || false
                    font.pixelSize: Theme.fontSize("body")
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    color: isSelected
                        ? Theme.color("accent")
                        : (fontDelegate.hovered ? Theme.color("accentSoft") : "transparent")
                    radius: Theme.radiusControl
                    Behavior on color {
                        ColorAnimation { duration: Theme.durationSm; easing.type: Easing.OutCubic }
                    }
                }

                onClicked: {
                    root._family = rowFamily
                    root._style  = rowStyle
                }
            }
        }

        // ── Divider ─────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.color("panelBorder")
        }

        // ── Preview ──────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 44
            color: "transparent"

            Text {
                anchors.fill: parent
                anchors.margins: 10
                text: qsTr("The wizard quickly jinxed the gnomes before they")
                font.family:    root._family.length > 0 ? root._family : Theme.fontFamilyUi
                font.pixelSize: root._size
                font.italic:    styleIsItalic(root._style)
                color: Theme.color("textPrimary")
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                clip: true

                function styleIsItalic(s) {
                    var sl = (s || "").toLowerCase()
                    return sl.indexOf("italic") >= 0 || sl.indexOf("oblique") >= 0
                }
            }
        }

        // ── Divider ─────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.color("panelBorder")
        }

        // ── Size controls ────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 8

            Label {
                text: qsTr("Size")
                font.pixelSize: Theme.fontSize("small")
                color: Theme.color("textSecondary")
            }

            Slider {
                id: sizeSlider
                Layout.fillWidth: true
                from: 6; to: 72; stepSize: 1
                value: root._size
                onMoved: root._size = Math.round(value)
            }

            // Numeric display with +/- buttons
            RowLayout {
                spacing: 0

                Button {
                    flat: true
                    text: "−"
                    font.pixelSize: Theme.fontSize("body")
                    implicitWidth: 28
                    implicitHeight: 28
                    onClicked: root._size = Math.max(6, root._size - 1)
                }

                Label {
                    text: root._size
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textPrimary")
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 28
                }

                Button {
                    flat: true
                    text: "+"
                    font.pixelSize: Theme.fontSize("body")
                    implicitWidth: 28
                    implicitHeight: 28
                    onClicked: root._size = Math.min(72, root._size + 1)
                }
            }
        }

        // Bottom margin
        Item { Layout.preferredHeight: 4 }
    }
}
