import QtQuick
import Rfvk

// Карточка аккаунта в сайдбаре: цветной аватар (инициалы), ник, строка
// «способ входа · логин», бейдж состояния сессии справа. Выделяется при выборе;
// при выделении нескольких на аватаре появляется отметка-галочка.
Rectangle {
    id: card
    property string accId: ""
    property string nick: ""
    property string login: ""
    property string loginKind: ""
    property int sessionState: App.None
    property string initials: "?"
    property bool selected: false
    property bool multi: false   // выделено несколько аккаунтов
    property bool compact: false

    // modifiers — Qt.ControlModifier / Qt.ShiftModifier (мультивыбор).
    signal clicked(int modifiers)
    signal activated() // двойной клик — запуск под этим аккаунтом

    readonly property color avColor: Theme.avatarColor(accId)

    implicitHeight: 64
    radius: Theme.rCard
    color: selected ? Theme.badgeFill(Theme.accent) : (area.containsMouse ? Theme.raised : "transparent")
    border.width: selected ? 1 : 0
    border.color: Theme.accent
    Behavior on color { ColorAnimation { duration: 90 } }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: (mouse) => card.clicked(mouse.modifiers)
        onDoubleClicked: (mouse) => {
            if (!(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)))
                card.activated()
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: Theme.s12
        spacing: Theme.s12

        // Аватар — цветной скруглённый квадрат с инициалами.
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 40; height: 40; radius: Theme.rCard
            color: Theme.avatarFill(card.avColor)
            AppText {
                anchors.centerIn: parent
                text: card.initials
                role: "bodyStrong"
                color: card.avColor
            }
            // Отметка выделения (режим нескольких аккаунтов).
            Rectangle {
                visible: card.multi && card.selected
                anchors.right: parent.right; anchors.rightMargin: -5
                anchors.bottom: parent.bottom; anchors.bottomMargin: -5
                width: 18; height: 18; radius: 9
                color: Theme.accent
                border.width: 2; border.color: Theme.surface
                Icon { anchors.centerIn: parent; name: "check"; color: Theme.accentInk; size: 11 }
            }
        }

        // Ник + строка входа (занимает оставшееся место до бейджа).
        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 40 - Theme.s12 - badge.width - Theme.s8
            spacing: 2
            visible: !card.compact
            AppText {
                width: parent.width
                text: card.nick
                role: "bodyStrong"
                elide: Text.ElideRight
            }
            AppText {
                width: parent.width
                text: Strings.loginKindLabel(card.loginKind) + " · " + card.login
                role: "small"
                color: Theme.muted
                elide: Text.ElideRight
            }
        }

        // Бейдж состояния справа.
        StatusBadge {
            id: badge
            anchors.verticalCenter: parent.verticalCenter
            state: card.sessionState
            dotOnly: card.compact
        }
    }
}
