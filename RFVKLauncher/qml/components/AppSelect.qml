import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Выпадающий список в стиле поля ввода (raised фон, обводка, акцент фокуса).
// model — массив объектов { label, value }; выбор — currentIndex / activated.
ComboBox {
    id: control
    textRole: "label"
    valueRole: "value"

    implicitHeight: Theme.hField
    leftPadding: Theme.s12
    rightPadding: Theme.s12 + 20
    font.family: Theme.fontUI
    font.pixelSize: 14
    hoverEnabled: true

    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

    background: Rectangle {
        radius: Theme.rField
        color: Theme.raised
        opacity: control.enabled ? 1 : 0.6
        border.width: control.visualFocus || control.popup.visible ? 2 : 1
        border.color: control.visualFocus || control.popup.visible ? Theme.accent
                    : control.hovered ? Theme.borderStrong : Theme.border
        Behavior on border.color { ColorAnimation { duration: 100 } }
    }

    contentItem: AppText {
        text: control.displayText
        role: "body"
        color: control.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        x: control.width - width - Theme.s12
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron"
        size: 14
        color: Theme.muted
        rotation: control.popup.visible ? 180 : 0
    }

    delegate: ItemDelegate {
        id: row
        required property var modelData
        required property int index
        width: control.width - 8
        height: 36
        leftPadding: Theme.s12
        highlighted: control.highlightedIndex === index
        contentItem: AppText {
            text: row.modelData.label
            role: "body"
            color: control.currentIndex === row.index ? Theme.accentText : Theme.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.rField - 2
            color: row.highlighted ? Theme.border : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }
        background: Rectangle {
            radius: Theme.rField
            color: Theme.surface
            border.width: 1
            border.color: Theme.borderStrong
        }
    }
}
