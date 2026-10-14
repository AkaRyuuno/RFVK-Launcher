import QtQuick
import Rfvk

// Бейдж состояния сессии: точка + текст, заливка цветом статуса ~13%.
// dotOnly — компактный режим (только точка, текст в тултипе) для узкого окна.
Row {
    id: badge
    property int state: App.None
    property bool dotOnly: false
    readonly property color c: Theme.statusColor(state)

    spacing: Theme.s4

    Rectangle {
        id: pill
        anchors.verticalCenter: parent.verticalCenter
        height: 22
        width: badge.dotOnly ? 22 : dotRow.implicitWidth + 2 * Theme.s8
        radius: Theme.rBadge
        color: Theme.badgeFill(badge.c)

        Row {
            id: dotRow
            anchors.centerIn: parent
            spacing: Theme.s4
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 8; height: 8; radius: 4
                color: badge.c
            }
            AppText {
                anchors.verticalCenter: parent.verticalCenter
                visible: !badge.dotOnly
                text: Strings.statusBadge(badge.state)
                role: "caption"
                font.weight: Font.DemiBold
                color: badge.c
            }
        }
    }
}
