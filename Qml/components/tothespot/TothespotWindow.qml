import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "." as TothespotComp

Window {
    id: root

    property var rootWindow: null
    property int panelHeight: 0
    property bool overlayVisible: false
    property bool showDebugInfo: false
    property var searchProfileMeta: ({})
    property var telemetryMeta: ({})
    property var telemetryLast: ({})
    property var providerStats: ({})
    property var previewData: ({})
    property string queryText: ""
    property var resultsModel: null
    property int selectedIndex: -1
    readonly property bool animatedVisible: overlayVisible || tothespotFrame.opacity > 0.01

    signal dismissRequested()
    signal queryTextChangedRequest(string text)
    signal selectedIndexChangedByUser(int indexValue)
    signal resultActivated(int indexValue)
    signal resultContextAction(int indexValue, string action)
    signal geometryEdited()
    signal openingRequested()

    function focusSearchField() {
        if (tothespotContent && tothespotContent.focusSearch) {
            tothespotContent.focusSearch()
        }
    }

    visible: !!rootWindow && !!rootWindow.visible && animatedVisible
    color: "transparent"
    flags: Qt.Popup | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    transientParent: rootWindow
    title: "Tothespot"
    width: Math.min(760, Math.max(540, (rootWindow ? rootWindow.width : 0) - 120))
    height: Math.min(520, Math.max(360, (rootWindow ? rootWindow.height : 0) - 180))
    x: (rootWindow ? rootWindow.x : 0) + Math.round(((rootWindow ? rootWindow.width : width) - width) / 2)
    y: (rootWindow ? rootWindow.y : 0)
       + Math.max(panelHeight + 12, Math.round((rootWindow ? rootWindow.height : height) * 0.08))

    onWidthChanged: geometryEdited()
    onHeightChanged: geometryEdited()
    onXChanged: geometryEdited()
    onYChanged: geometryEdited()
    onVisibleChanged: {
        if (visible && overlayVisible) {
            openingRequested()
            requestActivate()
        }
    }
    onActiveChanged: {
        if (!active && overlayVisible) {
            dismissRequested()
        }
    }
    onOverlayVisibleChanged: {
        if (!tothespotMotion.ready) {
            return
        }
        tothespotMotion.animateTo(overlayVisible ? 1.0 : 0.0)
    }

    QtObject {
        id: tothespotMotion
        property bool ready: false
        property real progress: 0.0

        function ensureChannel() {
            if (typeof MotionController === "undefined" || !MotionController) {
                return false
            }
            if (MotionController.channel !== "tothespot.reveal") {
                MotionController.channel = "tothespot.reveal"
            }
            if (MotionController.preset !== "snappy") {
                MotionController.preset = "snappy"
            }
            return true
        }

        function snapTo(stateValue) {
            if (!ensureChannel()) {
                progress = stateValue
                return
            }
            MotionController.cancelAndSettle(stateValue)
            progress = Number(MotionController.value || stateValue)
        }

        function animateTo(stateValue) {
            if (!ensureChannel()) {
                progress = stateValue
                return
            }
            MotionController.startFromCurrent(stateValue)
        }
    }

    Connections {
        target: (typeof MotionController !== "undefined" && MotionController) ? MotionController : null
        ignoreUnknownSignals: true
        function onValueChanged() {
            if (!tothespotMotion.ready) {
                return
            }
            if (MotionController.channel === "tothespot.reveal") {
                tothespotMotion.progress = Math.max(0, Math.min(1, Number(MotionController.value || 0)))
            }
        }
    }

    Item {
        id: tothespotFrame
        anchors.fill: parent
        opacity: tothespotMotion.progress
        scale: 0.97 + (0.03 * tothespotMotion.progress)
        transformOrigin: Item.Top

        TothespotComp.TothespotOverlay {
            id: tothespotContent
            anchors.fill: parent
            active: root.overlayVisible
            focus: active
            compactWindow: true
            showDebugInfo: root.showDebugInfo
            searchProfileMeta: root.searchProfileMeta
            telemetryMeta: root.telemetryMeta
            telemetryLast: root.telemetryLast
            providerStats: root.providerStats
            previewData: root.previewData
            queryText: root.queryText
            resultsModel: root.resultsModel
            selectedIndex: root.selectedIndex
            onQueryTextChangedRequest: function(text) {
                root.queryTextChangedRequest(text)
            }
            onSelectedIndexChanged: {
                root.selectedIndexChangedByUser(selectedIndex)
            }
            onResultActivated: function(indexValue) {
                root.resultActivated(indexValue)
            }
            onResultContextAction: function(indexValue, action) {
                root.resultContextAction(indexValue, action)
            }
            onDismissRequested: root.dismissRequested()
        }
    }

    Component.onCompleted: {
        tothespotMotion.ready = true
        tothespotMotion.snapTo(overlayVisible ? 1.0 : 0.0)
    }
}
