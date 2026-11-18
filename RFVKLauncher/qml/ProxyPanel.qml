import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Панель управления слотами маршрутизации (Direct + SOCKS5/HTTP прокси) и
// источниками прокси (списки по URL). Показывает емкость каждого слота (X/5),
// статус хук-библиотеки, запущенные PIDs и кнопки добавления/редактирования/
// проверки. Прокси, выданные из источников, — отдельные карточки без
// редактирования.
Item {
    id: root
    signal addRequested()
    signal editRequested(var slot)
    signal deleteRequested(string slotId, string slotName)
    signal addSourceRequested()
    signal editSourceRequested(var source)
    signal deleteSourceRequested(string sourceId, string sourceName)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s16

        // ── Заголовок панели + кнопки «Добавить источник» и «Добавить прокси» ──
        Item {
            Layout.fillWidth: true
            implicitHeight: Math.max(hdrLeft.height, addBtns.height)

            Column {
                id: hdrLeft
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: addBtns.left
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

            Row {
                id: addBtns
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8

                SecondaryButton {
                    kind: "outline"
                    iconName: "download"
                    text: Strings.addSource
                    onClicked: root.addSourceRequested()
                }
                SecondaryButton {
                    kind: "accent"
                    iconName: "plus"
                    text: Strings.addProxy
                    onClicked: root.addRequested()
                }
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

        // ── Слоты маршрутизации и источники ──
        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: listCol.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: listCol
                width: flick.width
                spacing: Theme.s12

                Repeater {
                    model: ProxyManager.model
                    delegate: ProxyCard {
                        width: listCol.width
                        onEditRequested: (slot) => root.editRequested(slot)
                        onDeleteRequested: (slotId, slotName) => root.deleteRequested(slotId, slotName)
                    }
                }

                Column {
                    width: parent.width
                    topPadding: Theme.s8
                    spacing: Theme.s4
                    visible: sourceRepeater.count > 0
                    AppText { text: Strings.sourcesTitle; role: "h3" }
                    AppText {
                        text: Strings.sourcesSubtitle
                        role: "small"
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }

                Repeater {
                    id: sourceRepeater
                    model: ProxyManager.sources
                    delegate: ProxySourceCard {
                        width: listCol.width
                        onEditRequested: (source) => root.editSourceRequested(source)
                        onDeleteRequested: (sourceId, sourceName) => root.deleteSourceRequested(sourceId, sourceName)
                    }
                }
            }
        }
    }
}
