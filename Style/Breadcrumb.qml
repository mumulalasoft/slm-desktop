import QtQuick
import QtQuick.Layouts
import Slm_Desktop

RowLayout {
    id: root

    property var model: []
    property int currentIndex: Math.max(0, model.length - 1)
    property bool allowNavigateToCurrent: false
    property string separatorText: ">"
    signal crumbClicked(int index, string text)

    spacing: Theme.metric("spacingXxs")

    Repeater {
        model: Array.isArray(root.model) ? root.model.length : 0

        delegate: RowLayout {
            required property int index
            spacing: Theme.metric("spacingXxs")

            ToolButton {
                text: String(root.model[index] || "")
                enabled: root.allowNavigateToCurrent || index !== root.currentIndex
                onClicked: root.crumbClicked(index, text)
            }

            Label {
                visible: index < (root.model.length - 1)
                text: root.separatorText
                color: Theme.color("textSecondary")
            }
        }
    }

    Item {
        Layout.fillWidth: true
    }
}
