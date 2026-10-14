import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Консоль журнала: построчно принимает App.logLine (время, текст, уровень),
// автоскролл вниз, цвет по уровню (обычный/успех/ошибка), кнопка «Очистить».
Rectangle {
    id: root
    radius: Theme.rCard
    color: Theme.consoleBg
    border.width: 1
    border.color: Theme.border
    clip: true

    ListModel { id: logModel }

    function color4(level) {
        return level === 1 ? Theme.ok : level === 2 ? Theme.err : Theme.text
    }

    Connections {
        target: App
        function onLogLine(time, text, level) {
            logModel.append({ t: time, m: text, lvl: level })
            listView.positionViewAtEnd()
        }
    }

    Column {
        anchors.fill: parent

        // Шапка консоли.
        Item {
            width: parent.width
            height: 36
            Row {
                anchors.left: parent.left; anchors.leftMargin: Theme.s12
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8
                AppText { anchors.verticalCenter: parent.verticalCenter; text: ">_"; role: "mono"; color: Theme.muted }
                AppText { anchors.verticalCenter: parent.verticalCenter; text: Strings.log; role: "overline"; color: Theme.muted }
            }
            SecondaryButton {
                anchors.right: parent.right; anchors.rightMargin: Theme.s8
                anchors.verticalCenter: parent.verticalCenter
                height: 26
                kind: "ghost"
                text: Strings.clear
                fontRole: "small"
                onClicked: logModel.clear()
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        ListView {
            id: listView
            width: parent.width
            height: parent.height - 36
            clip: true
            model: logModel
            topMargin: Theme.s8
            bottomMargin: Theme.s8
            leftMargin: Theme.s12
            rightMargin: Theme.s12
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Row {
                required property string t
                required property string m
                required property int lvl
                width: ListView.view.width - Theme.s12 * 2
                spacing: Theme.s8
                AppText { text: t; role: "mono"; color: Theme.muted }
                // Маркер по уровню: ✓ успех, · обычная, ✕ ошибка.
                AppText {
                    width: 10
                    text: lvl === 1 ? "✓" : lvl === 2 ? "✕" : "·"
                    role: "mono"
                    color: root.color4(lvl)
                }
                AppText {
                    width: parent.width - 80
                    text: m
                    role: "mono"
                    color: root.color4(lvl)
                    wrapMode: Text.WordWrap
                }
            }

            // Пусто.
            AppText {
                anchors.centerIn: parent
                visible: logModel.count === 0
                text: Strings.logEmpty
                role: "mono"
                color: Theme.muted
            }
        }
    }
}
