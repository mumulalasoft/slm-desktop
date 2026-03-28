import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle
import "../applet"


RowLayout {
    id: root

    property var fileManagerApi: null
    property var popupHost: null
    property int panelTextYOffset: 0
    property string timeText: ""
    property date nowDate: new Date()
    property bool includeDefaultIndicators: true
    property int indicatorSlotHeight: Theme.metric("controlHeightRegular")
    readonly property var monthNames: [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    ]
    readonly property var weekdayNames: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]
    property int selectedYear: nowDate.getFullYear()
    property int selectedMonth: nowDate.getMonth()
    property int selectedDay: nowDate.getDate()
    property double datePopupLastCloseMs: 0
    property int monthTransitionDirection: 1
    property real monthGridOffset: 0
    property real monthGridOpacity: 1.0
    property var _popupStates: ({})
    readonly property bool itemPopupOpen: {
        for (var key in _popupStates) {
            if (_popupStates[key] === true) return true
        }
        return false
    }
    readonly property bool anyPopupOpen: itemPopupOpen

    spacing: Theme.metric("spacingMd")

    Timer {
        id: nowDateTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.nowDate = new Date()
    }

    function syncCalendarToNow() {
        root.setSelectedDate(root.nowDate)
    }

    function yearToIndex(yearValue) {
        return Math.max(0, Math.min(150, Number(yearValue) - root.nowDate.getFullYear() + 75))
    }

    function daysInMonth(yearValue, monthValue) {
        return new Date(yearValue, monthValue + 1, 0).getDate()
    }

    function setSelectedDate(dateValue) {
        if (!dateValue || isNaN(dateValue.getTime())) {
            return
        }
        var prevKey = root.selectedYear * 12 + root.selectedMonth
        var nextYear = dateValue.getFullYear()
        var nextMonth = dateValue.getMonth()
        var nextDay = dateValue.getDate()
        root.selectedYear = dateValue.getFullYear()
        root.selectedMonth = dateValue.getMonth()
        root.selectedDay = dateValue.getDate()
        var nextKey = nextYear * 12 + nextMonth
        if (nextKey !== prevKey) {
            var direction = nextKey > prevKey ? 1 : -1
            root.monthTransitionDirection = direction
            root.monthGridOffset = 22 * direction
            root.monthGridOpacity = 0.0
            monthChangeAnim.restart()
            monthFadeInAnim.restart()
        } else {
            root.selectedDay = nextDay
        }
    }

    function shiftMonth(delta) {
        var d = root.selectedDisplayDate()
        d.setMonth(d.getMonth() + delta)
        root.setSelectedDate(d)
    }

    function moveSelection(deltaDays) {
        var d = root.selectedDisplayDate()
        d.setDate(d.getDate() + deltaDays)
        root.setSelectedDate(d)
    }

    function clampSelectedDay() {
        var maxDay = daysInMonth(root.selectedYear, root.selectedMonth)
        if (root.selectedDay < 1) {
            root.selectedDay = 1
        } else if (root.selectedDay > maxDay) {
            root.selectedDay = maxDay
        }
    }

    function firstWeekday(yearValue, monthValue) {
        return new Date(yearValue, monthValue, 1).getDay()
    }

    function dayForCell(indexValue) {
        var start = firstWeekday(root.selectedYear, root.selectedMonth)
        var day = indexValue - start + 1
        var maxDay = daysInMonth(root.selectedYear, root.selectedMonth)
        if (day < 1 || day > maxDay) {
            return 0
        }
        return day
    }

    function isTodayCell(dayValue) {
        if (dayValue <= 0) {
            return false
        }
        var d = root.nowDate
        return d.getFullYear() === root.selectedYear &&
               d.getMonth() === root.selectedMonth &&
               d.getDate() === dayValue
    }

    function isSelectedCell(dayValue) {
        if (dayValue <= 0) {
            return false
        }
        return dayValue === root.selectedDay
    }

    function selectedDisplayDate() {
        var maxDay = daysInMonth(root.selectedYear, root.selectedMonth)
        var day = Math.max(1, Math.min(maxDay, root.selectedDay))
        return new Date(root.selectedYear, root.selectedMonth, day)
    }

    function openDateTimeSettings() {
        var commands = [
            "gnome-control-center datetime",
            "plasma-open-settings kcm_clock",
            "kcmshell6 kcm_clock",
            "kcmshell5 kcm_clock",
            "xfce4-datetime-settings",
            "mate-time-admin"
        ]
        if (typeof AppExecutionGate === "undefined" || !AppExecutionGate || !AppExecutionGate.launchCommand) {
            return
        }
        for (var i = 0; i < commands.length; ++i) {
            if (AppExecutionGate.launchCommand(commands[i], "", "topbar.datetime")) {
                if (datetimeApplet && datetimeApplet.popup) datetimeApplet.popup.close()
                return
            }
        }
    }

    onSelectedMonthChanged: clampSelectedDay()
    onSelectedYearChanged: clampSelectedDay()

    NumberAnimation {
        id: monthChangeAnim
        target: root
        property: "monthGridOffset"
        to: 0
        duration: Theme.durationMd
        easing.type: Easing.OutCubic
        onStarted: {
            easing.type = root.monthTransitionDirection > 0 ? Easing.OutCubic : Easing.OutQuad
            duration = root.monthTransitionDirection > 0 ? Theme.durationMd : Theme.durationLg
        }
        onStopped: root.monthGridOpacity = 1.0
    }

    NumberAnimation {
        id: monthFadeInAnim
        target: root
        property: "monthGridOpacity"
        to: 1.0
        duration: Theme.durationMd
        easing.type: Easing.OutCubic
    }

    Repeater {
        model: (typeof StatusNotifierHost !== "undefined" && StatusNotifierHost)
               ? StatusNotifierHost.entries : []
        delegate: AppIndicatorItem {
            required property var modelData
            modelData: modelData
            host: (typeof StatusNotifierHost !== "undefined") ? StatusNotifierHost : null
            Layout.minimumWidth: implicitWidth
            Layout.preferredWidth: implicitWidth
            Layout.maximumWidth: implicitWidth
            Layout.preferredHeight: root.indicatorSlotHeight
            Layout.alignment: Qt.AlignVCenter
        }
    }

    Item { id: rootItem; visible: false; property bool popupOpen: false }

    Repeater {
        model: IndicatorRegistry.entries
        delegate: Loader {
            required property var modelData
            active: !!modelData && modelData.enabled !== false
            visible: !!modelData && modelData.visible !== false
            source: {
                var s = String(modelData && modelData.source ? modelData.source : "")
                if (s === "" || s.indexOf("://") >= 0 || s.indexOf("/") === 0) {
                    return s
                }
                if (s.indexOf("applet/") === 0) {
                    s = s.substring(7)
                }
                // Indicator applet QML files live in ../applet relative to this file.
                return Qt.resolvedUrl("../applet/" + s)
            }
            Layout.minimumWidth: implicitWidth
            Layout.preferredWidth: implicitWidth
            Layout.maximumWidth: implicitWidth
            Layout.preferredHeight: root.indicatorSlotHeight
            Layout.alignment: Qt.AlignVCenter

            onLoaded: {
                if (!item || !modelData) {
                    return
                }

                // Set properties from modelData.properties
                var props = modelData.properties || {}
                if ("manager" in item) {
                    item.manager = root
                }
                for (var key in props) {
                    if (item[key] !== undefined) {
                        try {
                            item[key] = props[key]
                        } catch(e) {
                            console.warn("IndicatorManager: Failed to set", key, "on", modelData.name)
                        }
                    }
                }

                // Track popup state
                if (item.popupOpen !== undefined) {
                    var updatePopup = function() {
                        var next = {}
                        for (var k in root._popupStates) next[k] = root._popupStates[k]
                        next[modelData.name] = !!item.popupOpen
                        root._popupStates = next
                    }
                    item.popupOpenChanged.connect(updatePopup)
                    updatePopup()
                }
            }
        }
    }

    Repeater {
        model: (typeof ExternalIndicatorRegistry !== "undefined" && ExternalIndicatorRegistry)
               ? ExternalIndicatorRegistry.entries : []
        delegate: Loader {
            required property var modelData
            active: !!modelData && modelData.enabled !== false
            visible: !!modelData && modelData.visible !== false
            source: modelData && modelData.source ? modelData.source : ""
            Layout.minimumWidth: implicitWidth
            Layout.preferredWidth: implicitWidth
            Layout.maximumWidth: implicitWidth
            Layout.preferredHeight: root.indicatorSlotHeight
            Layout.alignment: Qt.AlignVCenter

            onLoaded: {
                if (!item || !modelData) {
                    return
                }
                var props = modelData.properties || {}
                for (var key in props) {
                    if (item[key] !== undefined) {
                        try {
                            item[key] = props[key]
                        } catch(e) {
                            console.warn("IndicatorManager: Failed to set", key, "on external", modelData.name)
                        }
                    }
                }

                // Track popup state for external indicators too
                if (item.popupOpen !== undefined) {
                    var updatePopup = function() {
                        var next = {}
                        for (var k in root._popupStates) next[k] = root._popupStates[k]
                        next["ext-" + modelData.name] = !!item.popupOpen
                        root._popupStates = next
                    }
                    item.popupOpenChanged.connect(updatePopup)
                    updatePopup()
                }
            }
        }
    }


}
