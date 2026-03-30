import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: clockButtonHost
    property var manager: null
    property alias popup: dateTimePopup
    property alias popupOpen: dateTimePopup.opened
    implicitWidth: clockFrame.implicitWidth
    implicitHeight: manager.indicatorSlotHeight
    Layout.minimumWidth: implicitWidth
    Layout.preferredWidth: implicitWidth
    Layout.maximumWidth: implicitWidth
    Layout.preferredHeight: manager.indicatorSlotHeight
    Layout.alignment: Qt.AlignVCenter

    Rectangle {
        id: clockFrame
        implicitWidth: clockText.implicitWidth + (Theme.metric("spacingLg") + Theme.metric("spacingXxs"))
        implicitHeight: Theme.metric("controlHeightCompact")
        anchors.centerIn: parent
        radius: Theme.radiusControl
        color: clockMouse.containsMouse ? Theme.color("accentSoft") : "transparent"

        Text {
            id: clockText
            anchors.centerIn: parent
            anchors.verticalCenterOffset: manager.panelTextYOffset
            text: manager.timeText
            color: Theme.color("textOnGlass")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("bodyLarge")
            font.weight: Theme.fontWeight("semibold")
            renderType: Text.NativeRendering
        }

        MouseArea {
            id: clockMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: function(mouse) {
                if (dateTimePopup.opened || dateTimePopup.visible) {
                    manager.datePopupLastCloseMs = Date.now()
                    dateTimePopup.close()
                    mouse.accepted = true
                    return
                }
                if ((Date.now() - manager.datePopupLastCloseMs) < 220) {
                    mouse.accepted = true
                    return
                }
                manager.syncCalendarToNow()
                dateTimePopup.open()
                mouse.accepted = true
            }
        }
    }

    Popup {
        id: dateTimePopup
        modal: false
        focus: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 8
        width: Theme.metric("popupWidthL")
        x: Math.round(clockFrame.mapToGlobal(0, 0).x + (clockFrame.width - width) * 0.5)
        y: Math.round(clockFrame.mapToGlobal(0, 0).y + clockFrame.height + Theme.metric("spacingXs"))
        onOpened: manager.syncCalendarToNow()
        onOpenedChanged: {
            if (opened) {
                monthCombo.currentIndex = manager.selectedMonth
                yearCombo.currentIndex = manager.yearToIndex(manager.selectedYear)
            } else {
                manager.datePopupLastCloseMs = Date.now()
            }
        }

        background: DSStyle.PopupSurface {
            popupOpacity: Theme.popupSurfaceOpacityStrong
        }

        contentItem: ColumnLayout {
            spacing: Theme.metric("spacingSm")
            focus: dateTimePopup.opened
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Left) {
                    manager.moveSelection(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Right) {
                    manager.moveSelection(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Up) {
                    manager.moveSelection(-7)
                    event.accepted = true
                } else if (event.key === Qt.Key_Down) {
                    manager.moveSelection(7)
                    event.accepted = true
                } else if (event.key === Qt.Key_PageUp) {
                    manager.shiftMonth(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_PageDown) {
                    manager.shiftMonth(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Home) {
                    manager.syncCalendarToNow()
                    event.accepted = true
                } else if (event.key === Qt.Key_End) {
                    manager.selectedDay = manager.daysInMonth(manager.selectedYear, manager.selectedMonth)
                    event.accepted = true
                }
            }

            RowLayout {
                Layout.leftMargin: Theme.metric("spacingLg")
                Layout.rightMargin: Theme.metric("spacingLg")
                Layout.fillWidth: true
                spacing: Theme.metric("spacingXs")

                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: Theme.metric("controlHeightRegular")
                    radius: Theme.radiusCard
                    color: Theme.color("fileManagerControlBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("fileManagerControlBorder")

                    Row {
                        anchors.fill: parent
                        anchors.margins: Theme.metric("spacingXxs")
                        spacing: 0

                        Rectangle {
                            width: Math.floor((parent.width - 1) / 2)
                            height: parent.height
                            radius: Theme.radiusCard
                            color: prevMouse.pressed ? Theme.color("fileManagerControlActive")
                                                     : (prevMouse.containsMouse ? Theme.color("accentSoft")
                                                                                : "transparent")

                            Image {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: "image://themeicon/go-previous-symbolic?v=" +
                                        ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)
                                fillMode: Image.PreserveAspectFit
                                opacity: 1.0
                            }

                            MouseArea {
                                id: prevMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: manager.shiftMonth(-1)
                            }
                        }

                        Rectangle {
                            width: 1
                            height: parent.height - 6
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.color("panelBorder")
                            opacity: Theme.opacitySeparator
                        }

                        Rectangle {
                            width: Math.floor((parent.width - 1) / 2)
                            height: parent.height
                            radius: Theme.radiusCard
                            color: nextMouse.pressed ? Theme.color("fileManagerControlActive")
                                                     : (nextMouse.containsMouse ? Theme.color("accentSoft")
                                                                                : "transparent")

                            Image {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: "image://themeicon/go-next-symbolic?v=" +
                                        ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)
                                fillMode: Image.PreserveAspectFit
                                opacity: 1.0
                            }

                            MouseArea {
                                id: nextMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: manager.shiftMonth(1)
                            }
                        }
                    }
                }

                DSStyle.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: manager.monthNames[manager.selectedMonth] + " " + String(manager.selectedYear)
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("bold")
                }

                DSStyle.Button {
                    text: "Today"
                    onClicked: manager.syncCalendarToNow()
                }
            }

            RowLayout {
                Layout.leftMargin: Theme.metric("spacingLg")
                Layout.rightMargin: Theme.metric("spacingLg")
                Layout.fillWidth: true
                spacing: Theme.metric("spacingSm")

                DSStyle.ComboBox {
                    id: monthCombo
                    Layout.fillWidth: true
                    model: manager.monthNames
                    currentIndex: manager.selectedMonth
                    font.pixelSize: Theme.fontSize("small")
                    onActivated: manager.setSelectedDate(
                                     new Date(manager.selectedYear, currentIndex, manager.selectedDay))
                }

                DSStyle.ComboBox {
                    id: yearCombo
                    Layout.fillWidth: true
                    model: 151
                    textRole: ""
                    delegate: ItemDelegate {
                        required property int index
                        required property var modelData
                        text: String(manager.nowDate.getFullYear() - 75 + index)
                        width: parent ? parent.width : implicitWidth
                    }
                    contentItem: Text {
                        text: String(manager.nowDate.getFullYear() - 75 + yearCombo.currentIndex)
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: Theme.fontSize("small")
                        leftPadding: Theme.metric("spacingSm")
                        rightPadding: 24

                        elide: Text.ElideRight
                    }
                    onActivated: manager.setSelectedDate(
                                     new Date(manager.nowDate.getFullYear() - 75 + currentIndex,
                                              manager.selectedMonth,
                                              manager.selectedDay))
                }


            }

            Item {
                Layout.leftMargin: Theme.metric("spacingLg")
                Layout.rightMargin: Theme.metric("spacingLg")
                Layout.fillWidth: true
                implicitHeight: calendarGrid.implicitHeight
                clip: true

                GridLayout {
                    id: calendarGrid
                    width: parent.width
                    x: manager ? manager.monthGridOffset : 0
                    opacity: manager ? manager.monthGridOpacity : 1.0
                    columns: 7
                    columnSpacing: Theme.metric("spacingXxs")
                    rowSpacing: Theme.metric("spacingXxs")

                    Repeater {
                        model: manager ? manager.weekdayNames : []
                        delegate: DSStyle.Label {
                            required property var modelData
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: String(modelData)
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("tiny")
                            font.weight: Theme.fontWeight("bold")
                        }
                    }

                    Repeater {
                        model: 42
                        delegate: Rectangle {
                            required property int index
                            readonly property int dayValue: manager ? manager.dayForCell(index) : 0
                            readonly property bool isToday: manager ? manager.isTodayCell(dayValue) : false
                            readonly property bool isSelected: manager ? manager.isSelectedCell(dayValue) : false
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.metric("controlHeightRegular")
                            radius: Theme.radiusMd
                            color: isSelected ? (isToday ? Theme.color("accent") : Theme.color("accentSoft"))
                                              : (dayMouse.containsMouse ? Theme.color("accentSoft") : "transparent")
                            // border.width: dayValue > 0 ? 1 : 0
                            // border.color: isToday ? Theme.color("accent") : (isSelected ? Theme.color("accent") : Theme.color("panelBorder"))
                            opacity: dayValue > 0 ? 1.0 : 0.0
                            scale: dayMouse.pressed && dayValue > 0 ? 0.94 : 1.0
                            Behavior on scale {
                                NumberAnimation {
                                    duration: Theme.durationMicro
                                    easing.type: Theme.easingDefault
                                }
                            }

                            DSStyle.Label {
                                anchors.centerIn: parent
                                text: dayValue > 0 ? String(dayValue) : ""
                                color : isToday ? (isSelected ? Theme.color("accentText") : Theme.color("accent")) : Theme.color("textPrimary")
                                // color: isSelected ? (isToday ? Theme.color("accentText") : Theme.color("accent")) : Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("small")
                                font.weight: (isToday || isSelected) ? Theme.fontWeight("bold") : Theme.fontWeight("semibold")
                            }

                            MouseArea {
                                id: dayMouse
                                anchors.fill: parent
                                enabled: dayValue > 0
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: manager.selectedDay = dayValue
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.leftMargin: Theme.metric("spacingLg")
                Layout.rightMargin: Theme.metric("spacingLg")
                Layout.fillWidth: true
                height: 1
                color: Theme.color("menuBorder")
            }


            MenuItem {
                text: "Open Date & Time Settings"
                onClicked: manager.openDateTimeSettings()
                enabled: true
            }

        }
    }
}
