import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Левая зона: список сохранённых аккаунтов + кнопка «+» (добавить аккаунт).
Rectangle {
    id: sidebar
    property bool compact: false
    signal addRequested()

    color: Theme.surface
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }

    Column {
        anchors.fill: parent
        anchors.rightMargin: 1

        // Шапка сайдбара: «АККАУНТЫ» + счётчик + «+».
        Item {
            width: parent.width
            height: 56
            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.s20
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8
                visible: !sidebar.compact
                AppText { anchors.verticalCenter: parent.verticalCenter; text: Strings.accounts; role: "overline"; color: Theme.muted }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 18; width: Math.max(18, cnt.implicitWidth + 10); radius: Theme.rBadge
                    color: Theme.raised
                    AppText { id: cnt; anchors.centerIn: parent; text: list.count; role: "caption"; font.weight: Font.DemiBold; color: Theme.muted }
                }
            }
            // Accent-кнопка «+».
            Rectangle {
                id: addBtn
                anchors.right: parent.right
                anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                width: 36; height: 36; radius: Theme.rField
                color: addHover.hovered ? Theme.accentHover : Theme.accent
                Behavior on color { ColorAnimation { duration: 100 } }
                Icon { anchors.centerIn: parent; name: "plus"; color: Theme.accentInk; size: 20 }
                HoverHandler { id: addHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: sidebar.addRequested() }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        // Список аккаунтов.
        ListView {
            id: list
            width: parent.width
            height: parent.height - 56 - (hint.visible ? hint.height : 0) - footer.height
            clip: true
            spacing: Theme.s4
            topMargin: Theme.s12
            bottomMargin: Theme.s12
            leftMargin: Theme.s12
            rightMargin: Theme.s12
            model: App.accounts
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: AccountCard {
                required property var model
                width: ListView.view.width - Theme.s12 * 2
                compact: sidebar.compact
                accId: model.accId
                nick: model.nick
                login: model.login
                loginKind: model.loginKind
                sessionState: model.sessionState
                initials: model.initials
                selected: App.selectedIds.indexOf(model.accId) >= 0
                multi: App.selectedCount > 1
                // Обычный клик — один аккаунт; Ctrl — добавить/убрать; Shift — диапазон.
                onClicked: (mods) => {
                    if (mods & Qt.ShiftModifier) App.selectRange(model.accId)
                    else if (mods & Qt.ControlModifier) App.toggleSelect(model.accId)
                    else App.selectById(model.accId)
                }
                onActivated: { App.selectById(model.accId); App.launchSelected() }
            }

            // Пустой список.
            Column {
                anchors.centerIn: parent
                width: parent.width - Theme.s32
                spacing: Theme.s12
                visible: list.count === 0 && !sidebar.compact
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 56; height: 56; radius: Theme.rPill
                    color: Theme.raised; border.width: 1; border.color: Theme.border
                    Icon { anchors.centerIn: parent; name: "userplus"; color: Theme.muted; size: 26 }
                }
                AppText {
                    width: parent.width; horizontalAlignment: Text.AlignHCenter
                    text: Strings.sidebarEmptyTitle; role: "bodyStrong"
                }
                AppText {
                    width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    text: Strings.sidebarEmptyHint; role: "small"; color: Theme.muted
                }
                SecondaryButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    kind: "accent"; iconName: "plus"; text: Strings.addAccount; fontRole: "bodyStrong"
                    onClicked: sidebar.addRequested()
                }
            }
        }

        // Подсказка о мультивыборе (когда аккаунтов больше одного).
        Item {
            id: hint
            width: parent.width
            height: 36
            visible: list.count > 1 && !sidebar.compact
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
            AppText {
                anchors.left: parent.left; anchors.leftMargin: Theme.s20
                anchors.right: parent.right; anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                text: Strings.sidebarMultiHint
                role: "caption"; color: Theme.muted
                elide: Text.ElideRight
            }
        }

        // Подвал: версия лаунчера (из CMake, в релизе — из тега).
        Item {
            id: footer
            width: parent.width
            height: 28
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
            AppText {
                anchors.left: parent.left; anchors.leftMargin: Theme.s20
                anchors.right: parent.right; anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                text: Strings.versionLabel(Qt.application.version)
                role: "caption"; color: Theme.muted
                elide: Text.ElideRight
            }
        }
    }
}
