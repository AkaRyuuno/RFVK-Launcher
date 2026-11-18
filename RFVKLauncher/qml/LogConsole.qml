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

    readonly property alias textHasFocus: editor.activeFocus
    property var entries: []

    function escapeHtml(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                   .replace(/\r\n|\r|\n/g, "<br>")
    }

    function formatEntry(entry) {
        const color = entry.level === 1 ? Theme.ok : entry.level === 2 ? Theme.err : Theme.text
        const marker = entry.level === 1 ? "✓" : entry.level === 2 ? "✕" : "·"
        return '<p style="margin: 0; white-space: pre-wrap"><span style="color: ' + Theme.muted
               + '">' + escapeHtml(entry.time) + '</span><span style="color: ' + color
               + '"> ' + marker + ' ' + escapeHtml(entry.text) + '</span></p>'
    }

    function scrollToEnd() {
        if (editor.selectionStart === editor.selectionEnd)
            view.contentY = Math.max(0, view.contentHeight - view.height)
    }

    Connections {
        target: App
        function onLogLine(time, text, level) {
            const start = editor.selectionStart
            const end = editor.selectionEnd
            const follow = start === end && (view.atYEnd || !editor.length)
            const scrollPosition = view.contentY
            const entry = { time: time, text: text, level: level }
            root.entries.push(entry)
            editor.append(root.formatEntry(entry))

            if (start !== end)
                editor.select(start, end)
            if (follow) {
                Qt.callLater(root.scrollToEnd)
            } else {
                view.contentY = scrollPosition
            }
        }
    }

    Connections {
        target: Theme
        function onDarkChanged() {
            if (!editor.length)
                return
            const start = editor.selectionStart
            const end = editor.selectionEnd
            const scrollPosition = view.contentY
            editor.text = root.entries.map(root.formatEntry).join("")
            editor.select(start, end)
            view.contentY = scrollPosition
        }
    }

    Column {
        anchors.fill: parent

        // Шапка консоли.
        Item {
            id: header
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
                onClicked: {
                    root.entries = []
                    editor.clear()
                    view.contentY = 0
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        Flickable {
            id: view
            width: parent.width
            height: parent.height - header.height
            clip: true
            contentWidth: width
            contentHeight: editor.height
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            TextEdit {
                id: editor
                width: view.width
                height: Math.max(view.height, contentHeight + topPadding + bottomPadding)
                padding: Theme.s8
                leftPadding: Theme.s12
                rightPadding: Theme.s16
                font.family: Theme.fontMono
                font.pixelSize: 13
                renderType: TextEdit.QtRendering
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                readOnly: true
                selectByMouse: true
                persistentSelection: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: Theme.accentInk

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    enabled: editor.length > 0
                    onTapped: (eventPoint) => {
                        editor.forceActiveFocus(Qt.MouseFocusReason)
                        logMenu.popup(editor, eventPoint.position.x, eventPoint.position.y)
                    }
                }
                AppMenu {
                    id: logMenu
                    Action {
                        text: Strings.textCopy
                        enabled: editor.selectionStart !== editor.selectionEnd
                        onTriggered: editor.copy()
                    }
                    Action {
                        text: Strings.textSelectAll
                        onTriggered: editor.selectAll()
                    }
                }
            }

            AppText {
                y: Math.round((view.height - height) / 2)
                width: view.width - 2 * Theme.s12
                x: Theme.s12
                visible: !editor.length
                text: Strings.logEmpty
                role: "mono"
                color: Theme.muted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
