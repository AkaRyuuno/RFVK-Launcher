import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Главная кнопка. Состояния:
//  • обычное — акцентная заливка;
//  • busy — тёмная заливка, спиннер + текст шага + счётчик «N/M»;
//  • retry — красная обводка (повтор после ошибки).
AbstractButton {
    id: control
    property string iconName: "play"
    property bool busy: false
    property string busyText: ""
    property string counter: ""
    property bool retry: false
    property bool wrapText: false // переносить подпись на две строки (узкая кнопка)

    implicitHeight: Theme.hMain
    implicitWidth: Math.max(220, content.implicitWidth + 2 * Theme.s24)
    enabled: !busy
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

    background: Rectangle {
        radius: Theme.rMain
        color: control.busy ? Theme.raised
             : control.retry ? (control.hovered ? Qt.rgba(Theme.err.r, Theme.err.g, Theme.err.b, 0.12) : "transparent")
             : control.down ? Theme.accentPress
             : control.hovered ? Theme.accentHover
             : Theme.accent
        border.width: control.busy ? 1 : control.retry ? 1 : 0
        border.color: control.retry ? Theme.err : Theme.border
        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            anchors.fill: parent; anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"; border.width: 2; border.color: Theme.accent
            visible: control.visualFocus
        }
    }

    readonly property color fg: control.retry ? Theme.err : control.busy ? Theme.text : Theme.accentInk

    contentItem: Item {
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight
        Row {
            id: content
            anchors.centerIn: parent
            spacing: Theme.s8
            Spinner {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.busy; running: control.busy; color: Theme.accent
            }
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.busy && control.iconName.length > 0
                name: control.iconName
                color: control.fg
                size: 18
            }
            AppText {
                anchors.verticalCenter: parent.verticalCenter
                text: control.busy && control.busyText.length ? control.busyText : control.text
                role: "h3"
                color: control.fg
                horizontalAlignment: Text.AlignHCenter
                wrapMode: control.wrapText ? Text.WordWrap : Text.NoWrap
                maximumLineCount: 2
                lineHeight: control.wrapText ? 0.95 : 1.0
                width: control.wrapText
                       ? control.width - 2 * Theme.s16 - (control.iconName.length ? 18 + Theme.s8 : 0)
                       : implicitWidth
            }
            AppText {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.busy && control.counter.length > 0
                text: control.counter
                role: "small"
                color: Theme.muted
            }
        }
    }
}
