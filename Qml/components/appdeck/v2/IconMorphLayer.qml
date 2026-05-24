// AppDeck v2 icon morph layer (docs/APPDECK.md §7).
// Renders ONE delegate per app whose (x,y) is pointLerp(dockAnchor,
// gridAnchor, morphProgress) and whose scale is lerp(dockIconScale,
// gridIconScale). This is the layer the spec demands: icons literally fly
// from the dock to the grid and back, instead of fading in/out as two
// disjoint delegate trees.
//
// Tahap 4 lands the scaffold but is OFF by default (AppDeckTokens
// .iconMorphEnabled = false). When enabled, AppDeck.qml and AppDeckGridView
// .qml render only spacers for their icons, and this layer paints them.
import QtQuick 2.15

Item {
    id: iconLayer
    anchors.fill: parent

    property var appsModel: null      // dock-pinned model (initial Tahap 4 scope)
    property var geometry: null       // AppDeckGeometry
    property var tokens: null         // AppDeckTokens
    property real morphProgress: 0.0
    property bool enabledLayer: false // mirrors AppDeckTokens.iconMorphEnabled

    visible: enabledLayer
    enabled: enabledLayer

    readonly property real iconScale: geometry
                ? geometry.lerp(tokens ? tokens.dockIconScale : 1.0,
                                tokens ? tokens.gridIconScale : 1.0,
                                morphProgress)
                : 1.0

    signal appActivated(var appData)

    Repeater {
        id: iconRepeater
        model: iconLayer.enabledLayer && iconLayer.appsModel ? iconLayer.appsModel : 0

        delegate: AppDeckIconDelegate {
            id: iconDel
            // Anchor identity: dock uses the icon's stable id; grid uses
            // index. AppDeckGeometry.dockAnchorFor falls back to dockRect
            // center, and gridAnchorFor falls back to gridRect center, so
            // missing anchors don't teleport icons off-screen.
            readonly property string itemId: model && model.desktopFile
                                              ? String(model.desktopFile)
                                              : (model && model.name
                                                 ? String(model.name)
                                                 : String(index))
            readonly property point dockAnchor: iconLayer.geometry
                                                ? iconLayer.geometry.dockAnchorFor(itemId)
                                                : Qt.point(0, 0)
            readonly property point gridAnchor: iconLayer.geometry
                                                ? iconLayer.geometry.gridAnchorFor(index)
                                                : Qt.point(0, 0)
            readonly property point currentAnchor: iconLayer.geometry
                                                ? iconLayer.geometry.pointLerp(dockAnchor,
                                                                               gridAnchor,
                                                                               iconLayer.morphProgress)
                                                : Qt.point(0, 0)

            x: currentAnchor.x - width  / 2
            y: currentAnchor.y - height / 2
            scale: iconLayer.iconScale
            appData: model
            onActivated: function(d) { iconLayer.appActivated(d) }
        }
    }
}
