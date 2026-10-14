import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Правая зона при нескольких выделенных аккаунтах: сводный заголовок со
// стопкой аватаров и действиями (обновить / удалить — для всех выделенных),
// сетка мини-карточек выделенных аккаунтов и общая кнопка запуска.
Item {
    id: panel
    signal deleteRequested()

    readonly property var accs: App.selectedAccounts
    readonly property int n: accs.length
    // Аккаунты без действующей сессии — запуск для них не удастся.
    readonly property int noSession: {
        var k = 0
        for (var i = 0; i < accs.length; i++)
            if (accs[i].sessionState === App.Expired || accs[i].sessionState === App.None) k++
        return k
    }

    implicitHeight: content.implicitHeight

    Column {
        id: content
        width: parent.width
        spacing: Theme.s16

        // ── Заголовок: стопка аватаров + «Выбрано: N» + действия справа ──
        Item {
            width: parent.width
            implicitHeight: Math.max(hdrLeft.height, hdrRight.height)

            Row {
                id: hdrLeft
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - hdrRight.width - Theme.s16
                spacing: Theme.s16

                // До трёх аватаров внахлёст + «+K».
                Item {
                    id: stack
                    readonly property int shown: Math.min(panel.n, 3)
                    readonly property int extra: panel.n - shown
                    anchors.verticalCenter: parent.verticalCenter
                    width: 44 + (shown - 1) * 30 + (extra > 0 ? 30 : 0)
                    height: 44
                    Repeater {
                        model: stack.shown
                        Rectangle {
                            required property int index
                            readonly property var a: panel.accs[index]
                            readonly property color av: Theme.avatarColor(a ? a.id : "")
                            x: index * 30
                            z: 10 - index
                            width: 44; height: 44; radius: Theme.rCard + 2
                            color: Theme.bg
                            Rectangle {
                                anchors.fill: parent; anchors.margins: 2
                                radius: Theme.rCard
                                color: Theme.bg
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Theme.rCard
                                    color: Theme.avatarFill(av)
                                    AppText { anchors.centerIn: parent; text: a ? a.initials : ""; role: "bodyStrong"; color: av }
                                }
                            }
                        }
                    }
                    Rectangle {
                        visible: stack.extra > 0
                        x: stack.shown * 30
                        width: 44; height: 44; radius: Theme.rCard + 2
                        color: Theme.bg
                        Rectangle {
                            anchors.fill: parent; anchors.margins: 2
                            radius: Theme.rCard
                            color: Theme.raised; border.width: 1; border.color: Theme.border
                            AppText { anchors.centerIn: parent; text: "+" + stack.extra; role: "bodyStrong"; color: Theme.muted }
                        }
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - stack.width - Theme.s16
                    spacing: Theme.s4
                    AppText { width: parent.width; text: Strings.selectedTitle(panel.n); role: "title"; elide: Text.ElideRight }
                    AppText { width: parent.width; text: Strings.multiHint; role: "small"; color: Theme.muted; elide: Text.ElideRight }
                }
            }

            Row {
                id: hdrRight
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8
                SecondaryButton {
                    kind: "outline"; iconName: "refresh"; text: Strings.refresh
                    enabled: !App.busy
                    onClicked: App.refreshSelected()
                }
                // Удаление всех выделенных — компактная кнопка-иконка.
                Rectangle {
                    width: Theme.hField; height: Theme.hField; radius: Theme.rField
                    color: delHover.hovered && !App.busy ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.12) : "transparent"
                    border.width: 1; border.color: delHover.hovered && !App.busy ? Theme.danger : Theme.border
                    opacity: App.busy ? 0.5 : 1
                    Icon { anchors.centerIn: parent; name: "trash"; color: delHover.hovered && !App.busy ? Theme.danger : Theme.muted; size: 16 }
                    HoverHandler { id: delHover; cursorShape: App.busy ? Qt.ArrowCursor : Qt.PointingHandCursor }
                    TapHandler { enabled: !App.busy; onTapped: panel.deleteRequested() }
                }
            }
        }

        // ── Мини-карточки выделенных аккаунтов (2 колонки, прокрутка после 2 рядов) ──
        Flickable {
            id: grid
            readonly property int cardH: 56
            width: parent.width
            height: Math.min(flow.implicitHeight, 2 * cardH + Theme.s8)
            contentHeight: flow.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Flow {
                id: flow
                width: grid.width
                spacing: Theme.s8
                Repeater {
                    model: panel.accs
                    Rectangle {
                        id: mini
                        required property var modelData
                        readonly property var a: modelData
                        readonly property color av: Theme.avatarColor(a.id)
                        readonly property bool hasErr: a.error && a.error.length > 0
                        width: (flow.width - Theme.s8) / 2
                        height: grid.cardH
                        radius: Theme.rCard
                        color: miniHover.hovered ? Theme.raised : Theme.surface
                        border.width: 1
                        border.color: hasErr ? Qt.rgba(Theme.err.r, Theme.err.g, Theme.err.b, 0.6) : Theme.border
                        Behavior on color { ColorAnimation { duration: 90 } }

                        HoverHandler { id: miniHover; cursorShape: Qt.PointingHandCursor }
                        // Клик — открыть только этот аккаунт.
                        TapHandler { onTapped: App.selectById(mini.a.id) }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.s12
                            anchors.rightMargin: Theme.s8
                            spacing: Theme.s12
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 32; height: 32; radius: Theme.rField
                                color: Theme.avatarFill(mini.av)
                                AppText { anchors.centerIn: parent; text: mini.a.initials; role: "small"; font.weight: Font.DemiBold; color: mini.av }
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 32 - Theme.s12 - right.width - Theme.s8
                                spacing: 1
                                AppText { width: parent.width; text: mini.a.nick; role: "bodyStrong"; elide: Text.ElideRight }
                                AppText {
                                    width: parent.width
                                    text: mini.hasErr ? mini.a.error : (Strings.loginKindLabel(mini.a.loginKind) + " · " + mini.a.login)
                                    role: "caption"; color: mini.hasErr ? Theme.err : Theme.muted
                                    elide: Text.ElideRight
                                }
                            }
                            Row {
                                id: right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: Theme.s4
                                StatusBadge { anchors.verticalCenter: parent.verticalCenter; state: mini.a.sessionState }
                                // Убрать из выделения.
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 24; height: 24; radius: Theme.rBadge
                                    color: xHover.hovered ? Theme.border : "transparent"
                                    opacity: miniHover.hovered ? 1 : 0.35
                                    Icon { anchors.centerIn: parent; name: "cross"; color: Theme.muted; size: 12 }
                                    HoverHandler { id: xHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: App.toggleSelect(mini.a.id) }
                                    ToolTip.visible: xHover.hovered
                                    ToolTip.delay: 500
                                    ToolTip.text: Strings.removeFromSelection
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── Предупреждение: у части аккаунтов нет действующей сессии ──
        Rectangle {
            width: parent.width
            visible: panel.noSession > 0
            radius: Theme.rCard
            color: Theme.badgeFill(Theme.warn)
            border.width: 1; border.color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.4)
            implicitHeight: 40
            Icon {
                id: warnIcon
                anchors.left: parent.left; anchors.leftMargin: Theme.s12
                anchors.verticalCenter: parent.verticalCenter
                name: "warn"; color: Theme.warn; size: 16
            }
            AppText {
                anchors.left: warnIcon.right; anchors.leftMargin: Theme.s8
                anchors.right: parent.right; anchors.rightMargin: Theme.s12
                anchors.verticalCenter: parent.verticalCenter
                text: Strings.noSessionWarn(panel.noSession)
                role: "small"; color: Theme.muted; elide: Text.ElideRight
            }
        }

        // ── Главная кнопка: запуск всех выделенных ──
        PrimaryButton {
            width: parent.width
            text: Strings.launchN(panel.n)
            iconName: "play"
            busy: App.busy
            busyText: App.progressStep
            counter: App.batchTotal > 1 ? (App.batchIndex + "/" + App.batchTotal)
                   : App.progressTotal > 0 ? (App.progressIndex + "/" + App.progressTotal) : ""
            onClicked: App.launchSelected()
        }
    }
}
