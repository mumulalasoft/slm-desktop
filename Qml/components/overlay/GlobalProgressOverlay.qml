import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root
    width: 460
    height: 96

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusCard
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("windowCardBorder")
        opacity: Theme.cardSurfaceOpacity

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            Row {
                width: parent.width
                spacing: 8

                Text {
                    text: (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter)
                          ? String(GlobalProgressCenter.title || "Processing")
                          : "Processing"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("smallPlus")
                    font.weight: Theme.fontWeight("semibold")
                }

                Text {
                    text: (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter)
                          ? String(GlobalProgressCenter.detail || "")
                          : ""
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                    elide: Text.ElideRight
                    width: parent.width - 110
                }
            }

            DSStyle.ProgressBar {
                width: parent.width
                height: 10
                from: 0
                to: 1
                value: (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter)
                       ? Number(GlobalProgressCenter.progress || 0)
                       : 0
                indeterminate: (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter)
                               ? !!GlobalProgressCenter.indeterminate
                               : false
            }

            Row {
                width: parent.width
                spacing: 8

                DSStyle.Button {
                    text: (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter && GlobalProgressCenter.paused)
                          ? "Resume"
                          : "Pause"
                    enabled: (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter)
                             ? !!GlobalProgressCenter.canPause
                             : false
                    onClicked: {
                        if (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter &&
                                GlobalProgressCenter.requestPauseResume) {
                            GlobalProgressCenter.requestPauseResume()
                        }
                    }
                }

                DSStyle.Button {
                    text: "Cancel"
                    enabled: (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter)
                             ? !!GlobalProgressCenter.canCancel
                             : false
                    onClicked: {
                        if (typeof GlobalProgressCenter !== "undefined" && GlobalProgressCenter &&
                                GlobalProgressCenter.requestCancel) {
                            GlobalProgressCenter.requestCancel()
                        }
                    }
                }
            }
        }
    }
}
