import QtQuick 2.15

Item {
    id: root
    property bool active: false
    property color accentColor: "#4A90E2"
    property real ringWidth: 2.4

    implicitWidth: 28
    implicitHeight: 28
    opacity: active ? 1 : 0
    scale: active ? 1.0 : 0.98
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: active ? 180 : 150; easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        NumberAnimation { duration: 170; easing.type: Easing.OutQuad }
    }

    Canvas {
        id: arcCanvas
        anchors.fill: parent
        antialiasing: true
        renderTarget: Canvas.FramebufferObject

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cX = width / 2
            var cY = height / 2
            var r = Math.min(width, height) * 0.36

            ctx.lineWidth = root.ringWidth
            ctx.lineCap = "round"
            ctx.strokeStyle = root.accentColor
            ctx.beginPath()
            ctx.arc(cX, cY, r, Math.PI * 0.15, Math.PI * 1.55, false)
            ctx.stroke()

            ctx.globalAlpha = 0.25
            ctx.beginPath()
            ctx.arc(cX, cY, r, Math.PI * 1.55, Math.PI * 2.15, false)
            ctx.stroke()
        }
    }

    NumberAnimation on rotation {
        running: root.active
        loops: Animation.Infinite
        from: 0
        to: 360
        duration: 1100
        easing.type: Easing.Linear
    }

    SequentialAnimation on scale {
        running: root.active
        loops: Animation.Infinite
        NumberAnimation { to: 1.02; duration: 750; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 1.0; duration: 750; easing.type: Easing.InOutQuad }
    }
}
