import QtQuick 2.15
import QtQuick.Controls 2.15

Switch {
    id: control
    
    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 22
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 11
        color: control.checked ? "#34C759" : "#E5E5EA"
        
        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            y: 2
            width: 18
            height: 18
            radius: 9
            color: "white"
            
            Behavior on x {
                NumberAnimation { duration: 200; easing.type: Easing.OutBack }
            }
        }
    }
    
    contentItem: Text {
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.down ? "#17a81a" : "#21be2b"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
