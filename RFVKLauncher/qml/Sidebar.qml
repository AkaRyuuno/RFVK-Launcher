import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Левая зона: список сохранённых аккаунтов + кнопка «+» (добавить аккаунт).
Rectangle {
    id: sidebar
    property bool compact: false
    signal addRequested()
    signal exportRequested()
    signal deleteRequested()

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
            property string dragId: ""
            property string dropId: ""
            property bool dropAfter: false
            property real dropY: 0

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
            interactive: dragId === ""
            ScrollBar.vertical: ScrollBar {
                id: scrollBar
                policy: ScrollBar.AsNeeded
            }

            function updateDropTarget(position) {
                dropId = ""
                if (!contains(position)) return

                const point = mapToItem(contentItem, leftMargin + 1, position.y)
                let row = itemAt(point.x, point.y) || itemAt(point.x, point.y + spacing)
                if (!row) {
                    const first = itemAtIndex(0)
                    const last = itemAtIndex(count - 1)
                    if (first && point.y < first.y) row = first
                    else if (last && point.y >= last.y + last.height) row = last
                }
                if (!row || row.accId === dragId) return

                dropAfter = point.y >= row.y + row.height / 2
                dropY = mapFromItem(row, 0, dropAfter ? row.height : 0).y
                dropY += dropAfter ? spacing / 2 : -spacing / 2
                dropId = row.accId
            }

            DragHandler {
                id: reorder
                enabled: list.count > 1 && !App.busy && !scrollBar.pressed
                acceptedDevices: PointerDevice.Mouse
                acceptedModifiers: Qt.NoModifier
                target: null
                xAxis.enabled: false
                cursorShape: list.dragId.length > 0 ? Qt.ClosedHandCursor : undefined

                onActiveChanged: {
                    if (!active) return
                    const press = list.mapToItem(list.contentItem, centroid.pressPosition)
                    const row = list.itemAt(press.x, press.y)
                    if (row) {
                        list.dragId = row.accId
                        list.updateDropTarget(centroid.position)
                    }
                }
                onCentroidChanged: {
                    if (list.dragId.length > 0) list.updateDropTarget(centroid.position)
                }
                onGrabChanged: (transition, point) => {
                    if (transition !== PointerDevice.UngrabExclusive
                            && transition !== PointerDevice.CancelGrabExclusive) return
                    if (list.dragId.length > 0 && transition === PointerDevice.UngrabExclusive) {
                        list.updateDropTarget(list.mapFromItem(null, point.scenePosition))
                        if (list.dropId.length > 0)
                            App.moveAccount(list.dragId, list.dropId, list.dropAfter)
                    }
                    list.dragId = ""
                    list.dropId = ""
                }
            }

            Timer {
                interval: 16
                repeat: true
                running: list.dragId.length > 0 && list.contentHeight + list.topMargin + list.bottomMargin > list.height
                onTriggered: {
                    const point = reorder.centroid.position
                    if (!list.contains(point)) return
                    const edge = 32
                    const minY = list.originY - list.topMargin
                    const maxY = list.originY + list.contentHeight + list.bottomMargin - list.height
                    const delta = point.y < edge ? point.y - edge : Math.max(0, point.y - list.height + edge)
                    if (delta === 0) return
                    list.contentY = Math.max(minY, Math.min(maxY, list.contentY + delta / 4))
                    list.updateDropTarget(point)
                }
            }

            Rectangle {
                x: list.leftMargin
                y: list.dropY - height / 2
                width: list.width - list.leftMargin - list.rightMargin
                height: 2
                z: 2
                radius: 1
                color: Theme.accent
                visible: list.dropId.length > 0
            }

            delegate: AccountCard {
                id: row
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
                opacity: list.dragId === accId ? 0.5 : 1
                // Обычный клик — один аккаунт; Ctrl — добавить/убрать; Shift — диапазон.
                onClicked: (mods) => {
                    if (mods & Qt.ShiftModifier) App.selectRange(model.accId)
                    else if (mods & Qt.ControlModifier) App.toggleSelect(model.accId)
                    else App.selectById(model.accId)
                }
                onActivated: { App.selectById(model.accId); App.launchSelected() }
                onContextRequested: (x, y) => {
                    if (!row.selected) App.selectById(row.accId)
                    const position = row.mapToItem(list, x, y)
                    accountMenu.popup(list, position.x, position.y)
                }
            }

            AppMenu {
                id: accountMenu
                Action {
                    text: Strings.refresh
                    enabled: !App.busy
                    onTriggered: App.refreshSelected()
                }
                Action {
                    text: Strings.exportAccounts
                    enabled: !App.busy
                    onTriggered: sidebar.exportRequested()
                }
                Action {
                    text: Strings.confirmDeleteYes
                    enabled: !App.busy
                    onTriggered: sidebar.deleteRequested()
                }
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
