import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

// SuggestedZone — strips app yang sering dipakai.
// Dimuat dari appsModel.topApps(); dirender oleh host (AppDeckGridAppsView).
QtObject {
    id: root

    property var appsModel: null
    property int maxCount: 7
    property real revealProgress: 1.0

    readonly property var apps: {
        if (!appsModel || !appsModel.topApps) {
            return []
        }
        var raw = appsModel.topApps(maxCount) || []
        var result = []
        for (var i = 0; i < raw.length; ++i) {
            var e = raw[i] || {}
            if (!e.name && !e.executable) {
                continue
            }
            result.push({
                "name":        String(e.name || ""),
                "iconSource":  String(e.iconSource || ""),
                "iconName":    String(e.iconName || ""),
                "desktopId":   String(e.desktopId || ""),
                "desktopFile": String(e.desktopFile || ""),
                "executable":  String(e.executable || "")
            })
        }
        return result
    }

    readonly property bool hasApps: apps.length > 0
}
