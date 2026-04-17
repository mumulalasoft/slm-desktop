import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

Item {
    id: root
    anchors.fill: parent

    // ── Layout: left sidebar + right content ──────────────────────────────
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Left: service list ─────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                // Header
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Services")
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Font.DemiBold
                        color: Theme.color("textSecondary")
                    }

                    // Refresh button
                    ToolButton {
                        implicitWidth: 22
                        implicitHeight: 22
                        text: "↺"
                        font.pixelSize: Theme.fontSize("sm")
                        onClicked: if (DbusInspector) DbusInspector.refreshServices()
                        ToolTip.text: qsTr("Refresh service list")
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                    }
                }

                // Search filter
                TextField {
                    id: serviceFilter
                    Layout.fillWidth: true
                    implicitHeight: 26
                    placeholderText: qsTr("Filter…")
                    font.pixelSize: Theme.fontSize("sm")
                }

                // Show all toggle
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Show unique names")
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                    }
                    Switch {
                        implicitHeight: 18
                        checked: DbusInspector ? DbusInspector.showAllServices : false
                        onToggled: if (DbusInspector) DbusInspector.showAllServices = checked
                    }
                }

                // Service list
                ListView {
                    id: serviceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 1

                    model: {
                        const all = DbusInspector ? DbusInspector.services : []
                        const filter = serviceFilter.text.toLowerCase()
                        if (!filter) return all
                        return all.filter(s => s.toLowerCase().includes(filter))
                    }

                    delegate: ItemDelegate {
                        width: serviceList.width
                        height: 26
                        highlighted: DbusInspector &&
                                     DbusInspector.selectedService === modelData

                        background: Rectangle {
                            radius: 4
                            color: highlighted
                                   ? Theme.color("accent")
                                   : (hovered ? Theme.color("controlBgHover") : "transparent")
                        }

                        contentItem: Text {
                            text: modelData
                            font.pixelSize: Theme.fontSize("xs")
                            font.family: "monospace"
                            color: highlighted
                                   ? Theme.color("accentText")
                                   : Theme.color("textPrimary")
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (DbusInspector)
                                DbusInspector.selectedService = modelData
                        }
                    }
                }

                // Service count
                Text {
                    text: DbusInspector
                          ? qsTr("%1 services").arg(DbusInspector.services.length)
                          : ""
                    font.pixelSize: Theme.fontSize("xs")
                    color: Theme.color("textDisabled")
                }
            }
        }

        // ── Right: path + introspection content ────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Top bar: title + path selector ────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 48
                color: Theme.color("surface")
                border.color: Theme.color("panelBorder")
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10

                    Text {
                        text: DbusInspector && DbusInspector.selectedService.length > 0
                              ? DbusInspector.selectedService
                              : qsTr("Select a service")
                        font.pixelSize: Theme.fontSize("sm")
                        font.weight: Font.Medium
                        font.family: "monospace"
                        color: DbusInspector && DbusInspector.selectedService.length > 0
                               ? Theme.color("textPrimary")
                               : Theme.color("textDisabled")
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    // Path selector (ComboBox)
                    ComboBox {
                        id: pathCombo
                        implicitWidth: 200
                        implicitHeight: 28
                        visible: DbusInspector && DbusInspector.paths.length > 0
                        model: DbusInspector ? DbusInspector.paths : []
                        currentIndex: {
                            if (!DbusInspector) return -1
                            return DbusInspector.paths.indexOf(DbusInspector.selectedPath)
                        }
                        onActivated: {
                            if (DbusInspector)
                                DbusInspector.selectedPath = currentText
                        }
                        font.pixelSize: Theme.fontSize("xs")
                        font.family: "monospace"
                    }

                    // Loading indicator
                    BusyIndicator {
                        implicitWidth: 20
                        implicitHeight: 20
                        running: DbusInspector && DbusInspector.loading
                        visible: running
                    }

                    // Copy path button
                    ToolButton {
                        text: qsTr("Copy")
                        font.pixelSize: Theme.fontSize("xs")
                        visible: DbusInspector && DbusInspector.selectedPath.length > 0
                        onClicked: {
                            clipHelper.text = (DbusInspector ? DbusInspector.selectedService : "")
                                              + " " + (DbusInspector ? DbusInspector.selectedPath : "")
                        }
                        ToolTip.text: qsTr("Copy service + path to clipboard")
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                    }
                }
            }

            // Hidden clipboard helper
            TextEdit { id: clipHelper; visible: false
                onTextChanged: { selectAll(); copy() }
            }

            // ── Empty / placeholder states ─────────────────────────────────
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !DbusInspector || DbusInspector.selectedService.length === 0

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Select a service to introspect its objects.")
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textDisabled")
                }
            }

            // ── Error banner ───────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 36
                visible: DbusInspector && DbusInspector.introspectError.length > 0
                color: Qt.rgba(0.94, 0.2, 0.2, 0.08)
                border.color: Qt.rgba(0.94, 0.2, 0.2, 0.3)
                border.width: 1

                Text {
                    anchors.fill: parent
                    anchors.margins: 8
                    text: DbusInspector ? DbusInspector.introspectError : ""
                    font.pixelSize: Theme.fontSize("sm")
                    font.family: "monospace"
                    color: "#dc2626"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // ── Interface list ─────────────────────────────────────────────
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentHeight: ifaceCol.implicitHeight + 24
                clip: true
                visible: DbusInspector && DbusInspector.selectedService.length > 0

                ColumnLayout {
                    id: ifaceCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    // No interfaces yet (but service is selected)
                    Text {
                        Layout.fillWidth: true
                        visible: DbusInspector && DbusInspector.interfaces.length === 0
                                 && !DbusInspector.loading
                                 && DbusInspector.introspectError.length === 0
                        text: qsTr("No interfaces found at this path.")
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textDisabled")
                        topPadding: 12
                    }

                    // One section per interface
                    Repeater {
                        model: DbusInspector ? DbusInspector.interfaces : []

                        delegate: InterfaceSection {
                            Layout.fillWidth: true
                            ifaceData: modelData
                        }
                    }
                }
            }
        }
    }

    // ── Inline component: InterfaceSection ────────────────────────────────
    component InterfaceSection: Rectangle {
        id: section
        property var ifaceData: ({})

        implicitHeight: headerRow.height + (expanded ? contentCol.implicitHeight + 12 : 0)
        radius: Theme.radiusCard
        color: Theme.color("surface")
        border.color: Theme.color("panelBorder")
        border.width: 1
        clip: true

        property bool expanded: true

        Behavior on implicitHeight {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        // ── Header ─────────────────────────────────────────────────────────
        RowLayout {
            id: headerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 34
            spacing: 6

            // Expand chevron
            Text {
                leftPadding: 10
                text: section.expanded ? "▾" : "▸"
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
            }

            // Interface name
            Text {
                Layout.fillWidth: true
                text: section.ifaceData.name || ""
                font.pixelSize: Theme.fontSize("sm")
                font.family: "monospace"
                font.weight: Font.Medium
                color: Theme.color("textPrimary")
                elide: Text.ElideRight
            }

            // Badge counts
            Row {
                spacing: 4
                rightPadding: 10

                Repeater {
                    model: [
                        { count: (section.ifaceData.methods    || []).length, label: "M" },
                        { count: (section.ifaceData.properties || []).length, label: "P" },
                        { count: (section.ifaceData.signals    || []).length, label: "S" },
                    ]
                    delegate: Rectangle {
                        visible: modelData.count > 0
                        width: badgeLbl.implicitWidth + 8
                        height: 16
                        radius: 3
                        color: Qt.rgba(0.2, 0.6, 0.9, 0.1)
                        Text {
                            id: badgeLbl
                            anchors.centerIn: parent
                            text: modelData.label + modelData.count
                            font.pixelSize: Theme.fontSize("xs")
                            color: Theme.color("textSecondary")
                        }
                    }
                }
            }

            // Copy interface name
            ToolButton {
                text: "⎘"
                font.pixelSize: Theme.fontSize("xs")
                implicitWidth: 24
                implicitHeight: 24
                rightPadding: 6
                onClicked: { clipHelper.text = section.ifaceData.name || "" }
                ToolTip.text: qsTr("Copy interface name")
                ToolTip.visible: hovered
                ToolTip.delay: 500
            }
        }

        // Clickable header area
        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: headerRow.height
            onClicked: section.expanded = !section.expanded
            cursorShape: Qt.PointingHandCursor
        }

        // Separator
        Rectangle {
            anchors.top: headerRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.color("panelBorder")
            visible: section.expanded
        }

        // ── Content ────────────────────────────────────────────────────────
        ColumnLayout {
            id: contentCol
            anchors.top: headerRow.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8
            visible: section.expanded

            // Methods
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: (section.ifaceData.methods || []).length > 0

                Text {
                    text: qsTr("Methods")
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    color: Theme.color("textDisabled")
                    topPadding: 2
                }

                Repeater {
                    model: section.ifaceData.methods || []
                    delegate: MemberRow {
                        Layout.fillWidth: true
                        memberName: modelData.name
                        memberDetail: {
                            const inSig  = DbusInspector
                                           ? DbusInspector.formatArgList(modelData.args, "in")
                                           : "()"
                            const outSig = DbusInspector
                                           ? DbusInspector.formatArgList(modelData.args, "out")
                                           : "()"
                            return inSig + " → " + outSig
                        }
                        copyText: modelData.name + memberDetail
                        memberColor: Theme.color("textPrimary")
                    }
                }
            }

            // Properties
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: (section.ifaceData.properties || []).length > 0

                Text {
                    text: qsTr("Properties")
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    color: Theme.color("textDisabled")
                    topPadding: 2
                }

                Repeater {
                    model: section.ifaceData.properties || []
                    delegate: MemberRow {
                        Layout.fillWidth: true
                        memberName: modelData.name
                        memberDetail: modelData.type
                                      + (modelData.access ? "  [" + modelData.access + "]" : "")
                        copyText: modelData.name + "  " + memberDetail
                        memberColor: Theme.color("textSecondary")
                    }
                }
            }

            // Signals
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: (section.ifaceData.signals || []).length > 0

                Text {
                    text: qsTr("Signals")
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    color: Theme.color("textDisabled")
                    topPadding: 2
                }

                Repeater {
                    model: section.ifaceData.signals || []
                    delegate: MemberRow {
                        Layout.fillWidth: true
                        memberName: modelData.name
                        memberDetail: DbusInspector
                                      ? DbusInspector.formatArgList(modelData.args, "")
                                      : "()"
                        copyText: modelData.name + memberDetail
                        memberColor: "#9333ea"
                    }
                }
            }

            Item { height: 4 }
        }
    }

    // ── Inline component: MemberRow ───────────────────────────────────────
    component MemberRow: Rectangle {
        property string memberName:   ""
        property string memberDetail: ""
        property string copyText:     ""
        property color  memberColor:  Theme.color("textPrimary")

        implicitHeight: 22
        color: hoverHandler.hovered ? Theme.color("controlBgHover") : "transparent"
        radius: 3

        HoverHandler { id: hoverHandler }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 8

            Text {
                text: memberName
                font.pixelSize: Theme.fontSize("sm")
                font.family: "monospace"
                font.weight: Font.Medium
                color: memberColor
                Layout.preferredWidth: 200
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: memberDetail
                font.pixelSize: Theme.fontSize("xs")
                font.family: "monospace"
                color: Theme.color("textSecondary")
                elide: Text.ElideRight
            }

            ToolButton {
                text: "⎘"
                font.pixelSize: Theme.fontSize("xs")
                implicitWidth: 20
                implicitHeight: 20
                visible: hoverHandler.hovered
                onClicked: { clipHelper.text = copyText }
                ToolTip.text: qsTr("Copy to clipboard")
                ToolTip.visible: hovered
                ToolTip.delay: 400
            }
        }
    }
}
