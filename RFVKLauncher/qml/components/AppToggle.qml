import QtQuick
import Rfvk

// Переключатель-«галочка» в стиле дизайна: дорожка + бегунок, акцентный во
// включённом состоянии. checked/toggled — как у стандартного Switch.
Item {
    id: root
    property bool checked: false
    property bool enabled: true
    signal toggled(bool value)

    implicitWidth: 44
    implicitHeight: 24

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.raised
        border.width: 1
        border.color: root.checked ? Theme.accent : Theme.border
        opacity: root.enabled ? 1 : 0.5
        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            id: knob
            width: 18; height: 18; radius: 9
            y: (parent.height - height) / 2
            x: root.checked ? parent.width - width - 3 : 3
            color: root.checked ? Theme.accentInk : Theme.muted
            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
    }

    HoverHandler { cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    TapHandler {
        enabled: root.enabled
        onTapped: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
