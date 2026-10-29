import QtQuick
import Rfvk

// Простые векторные иконки (рисуются на Canvas, 24×24 сетка).
// Доступные name: play, refresh, trash, folder, key, plus, check, warn, cross,
// moon, sun, mail, phone, vk, download, sliders, userplus, lock, octagon, chevron.
Item {
    id: root
    property string name: "play"
    property color color: Theme.text
    property int size: 18
    property bool filled: false
    implicitWidth: size
    implicitHeight: size

    onColorChanged: cv.requestPaint()
    onNameChanged: cv.requestPaint()
    onFilledChanged: cv.requestPaint()

    Canvas {
        id: cv
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const s = Math.min(width, height) / 24.0
            ctx.scale(s, s)
            ctx.strokeStyle = root.color
            ctx.fillStyle = root.color
            ctx.lineWidth = 2
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            function stroke(fn) { ctx.beginPath(); fn(); ctx.stroke() }
            function fill(fn) { ctx.beginPath(); fn(); ctx.fill() }

            switch (root.name) {
            case "play":
                fill(function() { ctx.moveTo(8, 6); ctx.lineTo(18, 12); ctx.lineTo(8, 18); ctx.closePath() })
                break
            case "refresh":
                stroke(function() { ctx.arc(12, 12, 7, Math.PI * 0.2, Math.PI * 1.7) })
                fill(function() { ctx.moveTo(18.5, 5.5); ctx.lineTo(19.5, 10); ctx.lineTo(15, 9); ctx.closePath() })
                break
            case "trash":
                stroke(function() { ctx.moveTo(5, 7); ctx.lineTo(19, 7) })
                stroke(function() { ctx.moveTo(9, 7); ctx.lineTo(9, 5); ctx.lineTo(15, 5); ctx.lineTo(15, 7) })
                stroke(function() {
                    ctx.moveTo(6.5, 7); ctx.lineTo(7.5, 19); ctx.lineTo(16.5, 19); ctx.lineTo(17.5, 7)
                })
                stroke(function() { ctx.moveTo(10, 10); ctx.lineTo(10, 16) })
                stroke(function() { ctx.moveTo(14, 10); ctx.lineTo(14, 16) })
                break
            case "folder":
                stroke(function() {
                    ctx.moveTo(4, 7); ctx.lineTo(10, 7); ctx.lineTo(12, 9); ctx.lineTo(20, 9)
                    ctx.lineTo(20, 18); ctx.lineTo(4, 18); ctx.closePath()
                })
                break
            case "key":
                stroke(function() { ctx.arc(8, 9, 4, 0, Math.PI * 2) })
                stroke(function() { ctx.moveTo(11, 11.5); ctx.lineTo(19, 19) })
                stroke(function() { ctx.moveTo(16, 16); ctx.lineTo(18.5, 13.5) })
                break
            case "plus":
                stroke(function() { ctx.moveTo(12, 5); ctx.lineTo(12, 19) })
                stroke(function() { ctx.moveTo(5, 12); ctx.lineTo(19, 12) })
                break
            case "check":
                stroke(function() { ctx.moveTo(5, 12.5); ctx.lineTo(10, 17.5); ctx.lineTo(19, 6.5) })
                break
            case "warn":
                stroke(function() {
                    ctx.moveTo(12, 4); ctx.lineTo(21, 19); ctx.lineTo(3, 19); ctx.closePath()
                })
                stroke(function() { ctx.moveTo(12, 10); ctx.lineTo(12, 14) })
                fill(function() { ctx.arc(12, 16.5, 1, 0, Math.PI * 2) })
                break
            case "cross":
                stroke(function() { ctx.moveTo(6, 6); ctx.lineTo(18, 18) })
                stroke(function() { ctx.moveTo(18, 6); ctx.lineTo(6, 18) })
                break
            case "moon":
                fill(function() {
                    ctx.arc(13, 12, 8, Math.PI * 0.5, Math.PI * 1.5)
                    ctx.arc(10, 12, 9.5, Math.PI * 1.5, Math.PI * 0.5, true)
                    ctx.closePath()
                })
                break
            case "sun":
                stroke(function() { ctx.arc(12, 12, 4.5, 0, Math.PI * 2) })
                for (var i = 0; i < 8; i++) {
                    const a = i * Math.PI / 4
                    stroke(function() {
                        ctx.moveTo(12 + Math.cos(a) * 7.5, 12 + Math.sin(a) * 7.5)
                        ctx.lineTo(12 + Math.cos(a) * 9.5, 12 + Math.sin(a) * 9.5)
                    })
                }
                break
            case "mail":
                stroke(function() { ctx.rect(4, 6, 16, 12) })
                stroke(function() { ctx.moveTo(4, 7); ctx.lineTo(12, 13); ctx.lineTo(20, 7) })
                break
            case "phone":
                stroke(function() { ctx.rect(8, 4, 8, 16) })
                stroke(function() { ctx.moveTo(11, 17.5); ctx.lineTo(13, 17.5) })
                break
            case "vk":
                stroke(function() { ctx.rect(4, 4, 16, 16) })
                stroke(function() {
                    ctx.moveTo(7.5, 9); ctx.lineTo(9, 13); ctx.lineTo(11, 9)
                    ctx.moveTo(11, 9); ctx.lineTo(11, 15); ctx.lineTo(13.5, 11); ctx.lineTo(16.5, 15)
                })
                break
            case "download":
                stroke(function() { ctx.moveTo(12, 4); ctx.lineTo(12, 15) })
                stroke(function() { ctx.moveTo(7.5, 10.5); ctx.lineTo(12, 15); ctx.lineTo(16.5, 10.5) })
                stroke(function() { ctx.moveTo(5, 19); ctx.lineTo(19, 19) })
                break
            case "sliders":
                stroke(function() { ctx.moveTo(4, 8); ctx.lineTo(20, 8) })
                stroke(function() { ctx.moveTo(4, 16); ctx.lineTo(20, 16) })
                fill(function() { ctx.arc(9, 8, 2.6, 0, Math.PI * 2) })
                fill(function() { ctx.arc(15, 16, 2.6, 0, Math.PI * 2) })
                break
            case "userplus":
                stroke(function() { ctx.arc(9, 8, 3.5, 0, Math.PI * 2) })
                stroke(function() { ctx.moveTo(3, 19); ctx.arc(9, 19, 6, Math.PI, 0) })
                stroke(function() { ctx.moveTo(18, 7); ctx.lineTo(18, 13) })
                stroke(function() { ctx.moveTo(15, 10); ctx.lineTo(21, 10) })
                break
            case "chevron":
                stroke(function() { ctx.moveTo(6, 9); ctx.lineTo(12, 15); ctx.lineTo(18, 9) })
                break
            case "lock":
                stroke(function() { ctx.moveTo(8, 11); ctx.lineTo(8, 8); ctx.arc(12, 8, 4, Math.PI, 0); ctx.lineTo(16, 11) })
                stroke(function() { ctx.rect(6, 11, 12, 9) })
                break
            case "octagon":
                fill(function() {
                    var r = 9, cx = 12, cy = 12
                    for (var k = 0; k < 8; k++) {
                        var a = Math.PI / 8 + k * Math.PI / 4
                        var px = cx + r * Math.cos(a), py = cy + r * Math.sin(a)
                        if (k === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
                    }
                    ctx.closePath()
                })
                break
            }
        }
    }
}
