import QtQuick
import Rfvk

// Тосты: короткие уведомления снизу справа, исчезают сами через ~3.5 c.
Item {
    id: root
    anchors.fill: parent

    ListModel { id: model }
    property int seq: 0

    Connections {
        target: App
        function onToast(text, kind) {
            root.seq++
            model.append({ tid: root.seq, msg: text, kind: kind })
        }
    }

    function removeById(id) {
        for (var i = 0; i < model.count; i++)
            if (model.get(i).tid === id) { model.remove(i); return }
    }

    Column {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.s20
        spacing: Theme.s8

        Repeater {
            model: model
            delegate: Rectangle {
                id: toast
                required property int tid
                required property string msg
                required property int kind
                readonly property color accentC: kind === 1 ? Theme.ok : kind === 2 ? Theme.err : Theme.accent

                width: Math.min(360, rowT.implicitWidth + 2 * Theme.s16)
                height: rowT.implicitHeight + 2 * Theme.s12
                radius: Theme.rField
                color: Theme.raised
                border.width: 1
                border.color: Theme.borderStrong
                opacity: 0
                Component.onCompleted: opacity = 1
                Behavior on opacity { NumberAnimation { duration: 180 } }

                // Акцентная полоска слева.
                Rectangle { width: 3; height: parent.height - 2 * Theme.s8; y: Theme.s8; x: 2; radius: 2; color: toast.accentC }

                Row {
                    id: rowT
                    x: Theme.s16; y: Theme.s12
                    width: 360 - 2 * Theme.s16
                    spacing: Theme.s8
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: toast.kind === 1 ? "check" : toast.kind === 2 ? "warn" : "refresh"
                        color: toast.accentC
                        size: 16
                    }
                    AppText {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 24
                        text: toast.msg
                        role: "small"
                        wrapMode: Text.WordWrap
                    }
                }

                Timer { interval: 3500; running: true; onTriggered: root.removeById(toast.tid) }
                TapHandler { onTapped: root.removeById(toast.tid) }
            }
        }
    }
}
