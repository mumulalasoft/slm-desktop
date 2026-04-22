import QtQuick 2.15

QtObject {
    id: root

    property var appsModel: null

    readonly property int pinnedCount: {
        if (!appsModel || appsModel.count === undefined || !appsModel.get) {
            return 0
        }
        var count = 0
        for (var i = 0; i < Number(appsModel.count || 0); ++i) {
            var row = appsModel.get(i)
            if (row && row.pinned === true) {
                count += 1
            }
        }
        return count
    }
}
