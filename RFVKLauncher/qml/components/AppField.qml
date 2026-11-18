import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Rfvk

// Поле ввода в стиле дизайна (raised фон, обводка, фокус-акцент).
T.TextField {
    id: field
    property bool password: false

    implicitWidth: 200
    implicitHeight: Theme.hField
    leftPadding: Theme.s12
    rightPadding: Theme.s12
    color: Theme.text
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentInk
    font.family: Theme.fontUI
    font.pixelSize: 14
    renderType: TextInput.QtRendering
    echoMode: password ? TextInput.Password : TextInput.Normal
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    persistentSelection: editMenu.visible

    background: Rectangle {
        radius: Theme.rField
        color: Theme.raised
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? Theme.accent : (field.hovered ? Theme.borderStrong : Theme.border)
        Behavior on border.color { ColorAnimation { duration: 100 } }
    }

    Text {
        x: field.leftPadding
        width: field.width - field.leftPadding - field.rightPadding
        height: field.height
        text: field.placeholderText
        font: field.font
        color: field.placeholderTextColor
        renderType: Text.QtRendering
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        visible: !field.length && !field.preeditText
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => {
            field.forceActiveFocus(Qt.MouseFocusReason)
            editMenu.popup(field, eventPoint.position.x, eventPoint.position.y)
        }
    }

    AppMenu {
        id: editMenu
        readonly property bool hasSelection: field.selectionStart !== field.selectionEnd

        Action {
            text: Strings.textUndo
            enabled: !field.readOnly && field.canUndo
            onTriggered: field.undo()
        }
        Action {
            text: Strings.textRedo
            enabled: !field.readOnly && field.canRedo
            onTriggered: field.redo()
        }
        Action {
            text: Strings.textCut
            enabled: !field.readOnly && !field.password && editMenu.hasSelection
            onTriggered: field.cut()
        }
        Action {
            text: Strings.textCopy
            enabled: !field.password && editMenu.hasSelection
            onTriggered: field.copy()
        }
        Action {
            text: Strings.textPaste
            enabled: field.canPaste
            onTriggered: field.paste()
        }
        Action {
            text: Strings.textDelete
            enabled: !field.readOnly && editMenu.hasSelection
            onTriggered: field.remove(field.selectionStart, field.selectionEnd)
        }
        Action {
            text: Strings.textSelectAll
            enabled: field.length > 0
            onTriggered: field.selectAll()
        }
    }
}
