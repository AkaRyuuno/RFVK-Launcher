import QtQuick
import Rfvk

// Круговой индикатор (дуга), крутится пока running.
Item {
    id: root
    property color color: Theme.accentInk
    property int thickness: 2
    property bool running: true
    implicitWidth: 18
    implicitHeight: 18

    Canvas {
        id: cv
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const r = Math.min(width, height) / 2 - root.thickness
            ctx.lineWidth = root.thickness
            ctx.lineCap = "round"
            ctx.strokeStyle = root.color
            ctx.beginPath()
            // дуга 300°
            ctx.arc(width / 2, height / 2, r, 0, Math.PI * 1.7)
            ctx.stroke()
        }
    }
    RotationAnimator {
        target: root
        from: 0; to: 360
        duration: 900
        loops: Animation.Infinite
        running: root.running && root.visible
    }
}
