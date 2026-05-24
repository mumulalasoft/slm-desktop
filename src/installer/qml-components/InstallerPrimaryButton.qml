/*
 * InstallerPrimaryButton — the single primary action per installer screen.
 * §4 Color: accent fill, textOnAccent label. §4 Motion: pressed/hovered
 * states use durationFast (120ms) opacity transitions only.
 *
 * Supports a `loading` state for actions that take >300ms (re-enumeration,
 * verification). When loading, the label fades out and a centered spinner
 * fades in; the button stays the same size and stays disabled so the user
 * can't double-fire.
 */
import QtQuick
import "."

Item {
    id: root

    property string label: ""
    property bool enabled: true
    property bool loading: false

    signal clicked()

    implicitWidth: Math.max(180, labelText.implicitWidth + 48)
    implicitHeight: 40

    // Effective interactivity: disabled when explicitly disabled OR loading.
    readonly property bool _interactive: enabled && !loading

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: InstallerTheme.radiusControl
        color: InstallerTheme.accent
        opacity: root._interactive ? InstallerTheme.opacityShown
                                   : InstallerTheme.opacityDisabled

        Behavior on opacity {
            NumberAnimation { duration: InstallerTheme.durationFast }
        }

        // Hover wash — slight white overlay using textOnAccent.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: InstallerTheme.textOnAccent
            opacity: hover.hovered && root._interactive
                     ? InstallerTheme.opacityHoverWash
                     : InstallerTheme.opacityHidden
            Behavior on opacity {
                NumberAnimation { duration: InstallerTheme.durationFast }
            }
        }

        // Pressed wash — subtle textPrimary darken.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: InstallerTheme.textPrimary
            opacity: tap.pressed && root._interactive
                     ? InstallerTheme.opacityHoverWash
                     : InstallerTheme.opacityHidden
            Behavior on opacity {
                NumberAnimation { duration: InstallerTheme.durationFast }
            }
        }
    }

    // Focus ring — 3px outside, only visible when keyboard-focused.
    Rectangle {
        anchors {
            fill: parent
            margins: -3
        }
        radius: parent.radius + 3
        color: "transparent"
        border.width: InstallerTheme.borderAccent
        border.color: InstallerTheme.accent
        opacity: root.activeFocus ? InstallerTheme.opacityShown
                                  : InstallerTheme.opacityHidden
        Behavior on opacity {
            NumberAnimation { duration: InstallerTheme.durationFast }
        }
    }

    Text {
        id: labelText
        anchors.centerIn: parent
        text: root.label
        font.pixelSize: InstallerTheme.fontPxBody
        font.weight: InstallerTheme.weightMedium
        font.family: InstallerTheme.fontFamilyUi
        color: InstallerTheme.textOnAccent
        opacity: root.loading ? InstallerTheme.opacityHidden
                              : InstallerTheme.opacityShown
        Behavior on opacity {
            NumberAnimation { duration: InstallerTheme.durationFast }
        }
    }

    // Spinner — visible only during loading.
    Item {
        anchors.centerIn: parent
        width: 18
        height: 18
        opacity: root.loading ? InstallerTheme.opacityShown
                              : InstallerTheme.opacityHidden
        Behavior on opacity {
            NumberAnimation { duration: InstallerTheme.durationFast }
        }

        Rectangle {
            id: spinner
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: InstallerTheme.borderAccent
            border.color: InstallerTheme.textOnAccent
            opacity: InstallerTheme.opacitySeparator

            // Mask half the ring to make rotation visible.
            Rectangle {
                width: parent.width / 2
                height: parent.height
                color: InstallerTheme.accent
                anchors.right: parent.right
            }

            RotationAnimation on rotation {
                running: root.loading
                from: 0
                to: 360
                duration: 900
                loops: Animation.Infinite
            }
        }
    }

    HoverHandler {
        id: hover
        cursorShape: root._interactive ? Qt.PointingHandCursor : Qt.ForbiddenCursor
    }

    TapHandler {
        id: tap
        enabled: root._interactive
        onTapped: {
            root.forceActiveFocus()
            root.clicked()
        }
    }

    activeFocusOnTab: root._interactive
    Keys.onSpacePressed: if (root._interactive) root.clicked()
    Keys.onReturnPressed: if (root._interactive) root.clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.label
    Accessible.description: root.loading ? qsTr("Working…") : ""
    Accessible.onPressAction: if (root._interactive) root.clicked()
}
