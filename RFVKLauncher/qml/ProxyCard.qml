import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Карточка слота маршрутизации (Direct или Proxy) с индикатором вместимости (X/5 окон),
// пингом, списком активных PIDs и кнопками управления.
Rectangle {
    id: card
    readonly property var pids: (model && typeof model.activePids !== "undefined") ? model.activePids : []
    signal editRequested(var slot)
    signal deleteRequested(string slotId, string slotName)

    implicitWidth: parent ? parent.width : 500
    implicitHeight: mainCol.implicitHeight + Theme.s24
    radius: Theme.rCard
    color: Theme.surface
    border.width: 1
    border.color: cardHover.hovered ? Theme.borderStrong : Theme.border
    Behavior on border.color { ColorAnimation { duration: 100 } }

    HoverHandler { id: cardHover }

    Column {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.s16
        spacing: Theme.s12

        // Верхний ряд: бейдж типа + имя + статус емкости + действия
        RowLayout {
            width: parent.width
            spacing: Theme.s12

            // Иконка-бейдж протокола
            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: Theme.rField
                color: model.isDirect ? Theme.badgeFill(Theme.accent)
                     : model.type === 1 ? Theme.badgeFill(Theme.ok)
                     : Theme.badgeFill(Theme.warn)
                border.width: 1
                border.color: model.isDirect ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)
                            : model.type === 1 ? Qt.rgba(Theme.ok.r, Theme.ok.g, Theme.ok.b, 0.4)
                            : Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.4)

                Icon {
                    anchors.centerIn: parent
                    name: model.isDirect ? "server" : "globe"
                    size: 18
                    color: model.isDirect ? Theme.accent
                         : model.type === 1 ? Theme.ok
                         : Theme.warn
                }
            }

            // Название и детали
            Column {
                Layout.fillWidth: true
                spacing: 2

                Row {
                    spacing: Theme.s8
                    AppText {
                        text: model.name
                        role: "bodyStrong"
                        color: model.enabled ? Theme.text : Theme.muted
                    }
                    // Бейдж типа (SOCKS5 / HTTP / Прямой)
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 18
                        width: typeText.implicitWidth + Theme.s12
                        radius: Theme.rBadge
                        color: Theme.raised
                        AppText {
                            id: typeText
                            anchors.centerIn: parent
                            text: model.typeName
                            role: "caption"
                            font.weight: Font.DemiBold
                            color: Theme.muted
                        }
                    }
                    // Бейдж авторизации
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 18
                        width: authText.implicitWidth + Theme.s12
                        radius: Theme.rBadge
                        color: Theme.badgeFill(Theme.accent)
                        visible: !model.isDirect && model.authEnabled
                        AppText {
                            id: authText
                            anchors.centerIn: parent
                            text: Strings.proxyAuthBadge
                            role: "caption"
                            font.weight: Font.DemiBold
                            color: Theme.accent
                        }
                    }
                }

                AppText {
                    text: model.isDirect ? Strings.proxyDirectDesc
                        : (model.host + ":" + model.port + (model.authEnabled && model.username.length > 0 ? " · " + model.username : ""))
                    role: "small"
                    color: Theme.muted
                    elide: Text.ElideRight
                    width: parent.width
                }
                // Итог последней проверки (причина ошибки — здесь же).
                AppText {
                    visible: !model.isDirect && model.lastStatus.length > 0
                    text: model.lastStatus
                    role: "caption"
                    color: model.testOk ? Theme.muted : Theme.err
                    elide: Text.ElideRight
                    width: parent.width
                }
            }

            // Блок пинга
            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: 6
                visible: !model.isDirect

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7
                    height: 7
                    radius: 3.5
                    color: !model.testOk ? Theme.err : model.pingMs >= 0 ? Theme.ok : Theme.muted
                }
                AppText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: !model.testOk ? Strings.proxyPingError
                        : model.pingMs >= 0 ? Strings.proxyPingMs(model.pingMs)
                        : "—"
                    role: "caption"
                    color: !model.testOk ? Theme.err : model.pingMs >= 0 ? Theme.ok : Theme.muted
                }

                // Кнопка пинга
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    height: 28
                    radius: Theme.rField
                    color: testHover.hovered ? Theme.raised : "transparent"
                    border.width: 1
                    border.color: testHover.hovered ? Theme.borderStrong : Theme.border
                    Icon {
                        anchors.centerIn: parent
                        name: "refresh"
                        size: 13
                        color: testHover.hovered ? Theme.accent : Theme.muted
                    }
                    HoverHandler { id: testHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: ProxyManager.testProxy(model.slotId) }
                }
            }

            // Кнопки управления (только для добавленных прокси)
            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.s8
                visible: !model.isDirect

                // Вкл/Выкл переключатель
                AppToggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: model.enabled
                    onToggled: ProxyManager.setProxyEnabled(model.slotId, value)
                }

                // Редактировать
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 32
                    radius: Theme.rField
                    color: editHover.hovered ? Theme.raised : "transparent"
                    border.width: 1
                    border.color: editHover.hovered ? Theme.borderStrong : Theme.border
                    Icon {
                        anchors.centerIn: parent
                        name: "edit"
                        size: 14
                        color: editHover.hovered ? Theme.text : Theme.muted
                    }
                    HoverHandler { id: editHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: card.editRequested(model) }
                }

                // Удалить
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 32
                    radius: Theme.rField
                    opacity: model.activeClients > 0 ? 0.35 : 1.0
                    color: delHover.hovered && model.activeClients === 0
                         ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.12)
                         : "transparent"
                    border.width: 1
                    border.color: delHover.hovered && model.activeClients === 0 ? Theme.danger : Theme.border

                    Icon {
                        anchors.centerIn: parent
                        name: "trash"
                        size: 14
                        color: delHover.hovered && model.activeClients === 0 ? Theme.danger : Theme.muted
                    }
                    HoverHandler {
                        id: delHover
                        cursorShape: model.activeClients > 0 ? Qt.ForbiddenCursor : Qt.PointingHandCursor
                    }
                    TapHandler {
                        enabled: model.activeClients === 0
                        onTapped: card.deleteRequested(model.slotId, model.name)
                    }
                }
            }

            // Метка "По умолчанию" для прямого слота
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                height: 24
                width: directPillText.implicitWidth + Theme.s16
                radius: Theme.rBadge
                color: Theme.raised
                border.width: 1
                border.color: Theme.border
                visible: model.isDirect
                AppText {
                    id: directPillText
                    anchors.centerIn: parent
                    text: Strings.proxyDirectBadge
                    role: "caption"
                    color: Theme.muted
                }
            }
        }

        // Нижняя полоса: индикатор лимита слота (X/5) + прогрессбар + активные PID
        Rectangle {
            width: parent.width
            height: 1
            color: Theme.border
        }

        RowLayout {
            width: parent.width
            spacing: Theme.s12

            // Прогрессбар вместимости слота (0/5 .. 5/5)
            Column {
                Layout.preferredWidth: 160
                spacing: 4

                // Подпись слева, счётчик справа.
                Item {
                    width: parent.width
                    height: loadLabel.implicitHeight
                    AppText {
                        id: loadLabel
                        text: Strings.slotLoad
                        role: "caption"
                        color: Theme.muted
                    }
                    AppText {
                        anchors.right: parent.right
                        text: Strings.slotCapacityText(model.activeClients, model.maxClients)
                        role: "caption"
                        font.weight: Font.DemiBold
                        color: model.isFull ? Theme.err
                             : model.activeClients >= 4 ? Theme.warn
                             : model.activeClients > 0 ? Theme.ok
                             : Theme.muted
                    }
                }

                // Индикатор заполнения
                Rectangle {
                    width: parent.width
                    height: 6
                    radius: 3
                    color: Theme.raised

                    Rectangle {
                        height: parent.height
                        radius: 3
                        width: parent.width * Math.min(1.0, Math.max(0.0, model.activeClients / Math.max(1, model.maxClients)))
                        color: model.isFull ? Theme.err
                             : model.activeClients >= 4 ? Theme.warn
                             : Theme.ok
                        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    }
                }
            }

            // Список активных PID или статус
            Row {
                Layout.fillWidth: true
                spacing: Theme.s8
                Layout.alignment: Qt.AlignVCenter

                AppText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: model.activeClients > 0 ? Strings.slotPids : Strings.slotNoClients
                    role: "small"
                    color: Theme.muted
                }

                // Отображение бейджей запущенных PID
                Repeater {
                    model: card.pids
                    delegate: Rectangle {
                        height: 20
                        width: pidLabel.implicitWidth + 10
                        radius: Theme.rBadge
                        color: Theme.badgeFill(Theme.ok)
                        border.width: 1
                        border.color: Qt.rgba(Theme.ok.r, Theme.ok.g, Theme.ok.b, 0.3)

                        AppText {
                            id: pidLabel
                            anchors.centerIn: parent
                            text: modelData
                            role: "caption"
                            font.family: Theme.fontMono
                            font.weight: Font.Medium
                            color: Theme.ok
                        }
                    }
                }
            }

            // Бейдж "Заполнен", если 5/5
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                visible: model.isFull
                height: 22
                width: fullText.implicitWidth + Theme.s16
                radius: Theme.rBadge
                color: Theme.badgeFill(Theme.err)
                border.width: 1
                border.color: Qt.rgba(Theme.err.r, Theme.err.g, Theme.err.b, 0.4)
                AppText {
                    id: fullText
                    anchors.centerIn: parent
                    text: Strings.slotFull
                    role: "caption"
                    font.weight: Font.DemiBold
                    color: Theme.err
                }
            }
        }
    }
}
