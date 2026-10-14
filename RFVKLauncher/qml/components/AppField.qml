import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Поле ввода в стиле дизайна (raised фон, обводка, фокус-акцент).
TextField {
    id: field
    property bool password: false

    implicitHeight: Theme.hField
    leftPadding: Theme.s12
    rightPadding: Theme.s12
    color: Theme.text
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentInk
    font.family: Theme.fontUI
    font.pixelSize: 14
    echoMode: password ? TextInput.Password : TextInput.Normal
    verticalAlignment: TextInput.AlignVCenter

    background: Rectangle {
        radius: Theme.rField
        color: Theme.raised
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? Theme.accent : (field.hovered ? Theme.borderStrong : Theme.border)
        Behavior on border.color { ColorAnimation { duration: 100 } }
    }
}
