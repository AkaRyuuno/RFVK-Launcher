import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Вторичные кнопки. kind: "outline" (обводка), "raised" (заливка raised),
// "ghost" (только текст), "danger" (красная, для удаления).
AbstractButton {
    id: control
    property string kind: "outline"
    property string iconName: ""
    property string fontRole: "body"

    implicitHeight: Theme.hField
    implicitWidth: row.implicitWidth + 2 * Theme.s16
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    readonly property color baseText: kind === "danger" ? Theme.dangerText
                                    : kind === "accent" ? Theme.accentInk
                                    : kind === "ghost" ? Theme.muted
                                    : Theme.text

    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

    background: Rectangle {
        radius: Theme.rField
        border.width: control.kind === "outline" ? 1 : 0
        border.color: control.hovered ? Theme.borderStrong : Theme.border
        color: {
            if (!control.enabled) return control.kind === "ghost" ? "transparent" : Theme.raised
            if (control.kind === "danger") return control.down ? Qt.darker(Theme.danger, 1.2)
                                                : control.hovered ? Qt.lighter(Theme.danger, 1.1) : Theme.danger
            if (control.kind === "accent") return control.down ? Theme.accentPress
                                                : control.hovered ? Theme.accentHover : Theme.accent
            if (control.kind === "ghost") return control.hovered ? Theme.badgeFill(Theme.muted) : "transparent"
            if (control.kind === "raised") return control.hovered ? Theme.border : Theme.raised
            // outline
            return control.hovered ? Theme.raised : "transparent"
        }
        Behavior on color { ColorAnimation { duration: 100 } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
            visible: control.visualFocus
        }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: control.text.length > 0 && control.iconName.length > 0 ? Theme.s8 : 0
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
                name: control.iconName
                color: control.enabled ? control.baseText : Theme.muted
                size: 16
            }
            AppText {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.text.length > 0
                text: control.text
                role: control.fontRole
                color: control.enabled ? control.baseText : Theme.muted
            }
        }
    }
}
