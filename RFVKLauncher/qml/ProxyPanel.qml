import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Панель управления слотами маршрутизации (Direct + SOCKS5/HTTP прокси).
// Показывает емкость каждого слота (X/5), статус хук-библиотеки, запущенные PIDs
// и предоставляет кнопки добавления/редактирования/проверки.
Item {
    id: root
    signal addRequested()
    signal editRequested(var slot)
    signal deleteRequested(string slotId, string slotName)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s16

        // ── Заголовок панели + кнопка «+ Добавить прокси» ──
        Item {
            Layout.fillWidth: true
            implicitHeight: Math.max(hdrLeft.height, addBtn.height)

            Column {
                id: hdrLeft
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: addBtn.left
                anchors.rightMargin: Theme.s16
                spacing: Theme.s4

                AppText {
                    text: Strings.proxiesTitle
                    role: "h2"
                }
                AppText {
                    text: Strings.proxiesSubtitle
                    role: "small"
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }

            SecondaryButton {
                id: addBtn
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                kind: "accent"
                iconName: "plus"
                text: Strings.addProxy
                onClicked: root.addRequested()
            }
        }

        // ── Информационные карточки-сводки ──
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s12

            // Карточка: Всего активных окон игры
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: Theme.rCard
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Row {
                    anchors.centerIn: parent
                    spacing: Theme.s12
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28; height: 28; radius: 14
                        color: ProxyManager.totalActiveClients > 0 ? Theme.badgeFill(Theme.ok) : Theme.raised
                        Icon {
                            anchors.centerIn: parent
                            name: "play"
                            size: 14
                            color: ProxyManager.totalActiveClients > 0 ? Theme.ok : Theme.muted
                        }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        AppText {
                            text: Strings.proxyActiveWindows(ProxyManager.totalActiveClients)
                            role: "bodyStrong"
                        }
                        AppText {
                            text: ProxyManager.totalActiveClients > 0 ? Strings.slotsBusy : Strings.slotsIdle
                            role: "caption"
                            color: Theme.muted
                        }
                    }
                }
            }

            // Карточка: Доступно свободных слотов
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: Theme.rCard
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Row {
                    anchors.centerIn: parent
                    spacing: Theme.s12
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28; height: 28; radius: 14
                        color: ProxyManager.availableSlotsCount > 0 ? Theme.badgeFill(Theme.accent) : Theme.badgeFill(Theme.err)
                        Icon {
                            anchors.centerIn: parent
                            name: "server"
                            size: 14
                            color: ProxyManager.availableSlotsCount > 0 ? Theme.accent : Theme.err
                        }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        AppText {
                            text: Strings.proxyAvailableSlots(ProxyManager.availableSlotsCount)
                            role: "bodyStrong"
                        }
                        AppText {
                            text: ProxyManager.availableSlotsCount > 0 ? Strings.slotsReady : Strings.slotsAllFull
                            role: "caption"
                            color: ProxyManager.availableSlotsCount > 0 ? Theme.muted : Theme.err
                        }
                    }
                }
            }

            // Карточка: Статус rfvk_proxy.dll
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: Theme.rCard
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Row {
                    anchors.centerIn: parent
                    spacing: Theme.s12
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28; height: 28; radius: 14
                        color: ProxyManager.proxyDllAvailable ? Theme.badgeFill(Theme.ok) : Theme.badgeFill(Theme.err)
                        Icon {
                            anchors.centerIn: parent
                            name: ProxyManager.proxyDllAvailable ? "check" : "warn"
                            size: 14
                            color: ProxyManager.proxyDllAvailable ? Theme.ok : Theme.err
                        }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        AppText {
                            text: ProxyManager.proxyDllAvailable ? Strings.proxyDllReady : Strings.proxyDllMissing
                            role: "bodyStrong"
                            color: ProxyManager.proxyDllAvailable ? Theme.text : Theme.err
                        }
                        AppText {
                            text: ProxyManager.proxyDllAvailable ? Strings.proxyDllReadyHint : Strings.proxyDllMissingHint
                            role: "caption"
                            color: Theme.muted
                        }
                    }
                }
            }
        }

        // ── Список слотов маршрутизации ──
        ListView {
            id: slotList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.s12
            model: ProxyManager.model
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ProxyCard {
                width: slotList.width
                onEditRequested: (slot) => root.editRequested(slot)
                onDeleteRequested: (slotId, slotName) => root.deleteRequested(slotId, slotName)
            }
        }
    }
}
