import QtQuick
import QtQuick.Controls.Basic
import Rfvk

Menu {
    padding: Theme.s4
    margins: Theme.s4

    background: Rectangle {
        implicitWidth: 184
        radius: Theme.rField
        color: Theme.surface
        border.width: 1
        border.color: Theme.borderStrong
    }

    delegate: MenuItem {
        id: entry
        implicitHeight: 32
        leftPadding: Theme.s12
        rightPadding: Theme.s12

        contentItem: ButtonLabel {
            text: entry.text
            role: "small"
            color: entry.enabled ? Theme.text : Theme.muted
            opacity: entry.enabled ? 1 : 0.6
        }

        background: Rectangle {
            radius: Theme.rBadge
            color: entry.highlighted ? Theme.raised : "transparent"
        }
    }
}
