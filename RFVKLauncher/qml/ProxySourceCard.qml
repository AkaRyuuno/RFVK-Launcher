import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Карточка источника прокси (список по URL): счётчики рабочих / нерабочих
// адресов, ход проверки, занятость в игре и действия с источником целиком.
Rectangle {
    id: card
    signal editRequested(var source)
    signal deleteRequested(string sourceId, string sourceName)

    readonly property bool busy: model.fetching || model.checking

    implicitWidth: parent ? parent.width : 500
    implicitHeight: mainCol.implicitHeight + Theme.s24
    radius: Theme.rCard
    color: Theme.surface
    border.width: 1
    border.color: cardHover.hovered ? Theme.borderStrong : Theme.border
    Behavior on border.color { ColorAnimation { duration: 100 } }

    HoverHandler { id: cardHover }

    // Квадратная кнопка с иконкой и подсказкой.
    component IconButton: Rectangle {
        id: btn
        property string iconName
        property string tip
        property color hoverColor: Theme.text
        property bool active: true
        signal clicked()

        width: 32
        height: 32
        radius: Theme.rField
        opacity: active ? 1.0 : 0.35
        color: btnHover.hovered && active ? Theme.raised : "transparent"
        border.width: 1
        border.color: btnHover.hovered && active ? Theme.borderStrong : Theme.border
        Icon {
            anchors.centerIn: parent
            name: btn.iconName
            size: 14
            color: btnHover.hovered && btn.active ? btn.hoverColor : Theme.muted
        }
        HoverHandler {
            id: btnHover
            cursorShape: btn.active ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        }
        TapHandler {
            enabled: btn.active
            onTapped: btn.clicked()
        }
        ToolTip.visible: btnHover.hovered && tip.length > 0
        ToolTip.text: tip
        ToolTip.delay: 400
    }

    // Счётчик-бейдж.
    component Chip: Rectangle {
        id: chip
        property string text
        property color tone: Theme.muted
        height: 22
        width: chipText.implicitWidth + Theme.s16
        radius: Theme.rBadge
        color: Theme.badgeFill(tone)
        border.width: 1
        border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.3)
        AppText {
            id: chipText
            anchors.centerIn: parent
            text: chip.text
            role: "caption"
            font.weight: Font.DemiBold
            color: chip.tone
        }
    }

    Column {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.s16
        spacing: Theme.s12

        RowLayout {
            width: parent.width
            spacing: Theme.s12

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: Theme.rField
                color: Theme.badgeFill(Theme.accent)
                border.width: 1
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)
                Icon {
                    anchors.centerIn: parent
                    visible: !card.busy
                    name: "download"
                    size: 18
                    color: Theme.accent
                }
                Spinner {
                    anchors.centerIn: parent
                    visible: card.busy
                    running: card.busy
                    color: Theme.accent
                }
            }

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
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 18
                        width: typeText.implicitWidth + Theme.s12
                        radius: Theme.rBadge
                        color: Theme.raised
                        AppText {
                            id: typeText
                            anchors.centerIn: parent
                            text: model.defaultType === 2 ? "HTTP" : "SOCKS5"
                            role: "caption"
                            font.weight: Font.DemiBold
                            color: Theme.muted
                        }
                    }
                }
                AppText {
                    text: model.url
                    role: "small"
                    color: Theme.muted
                    elide: Text.ElideMiddle
                    width: parent.width
                }
            }

            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.s8

                AppToggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: model.enabled
                    onToggled: ProxyManager.setSourceEnabled(model.sourceId, value)
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "download"
                    tip: Strings.sourceRefresh
                    hoverColor: Theme.accent
                    active: !model.fetching
                    onClicked: ProxyManager.refreshSource(model.sourceId)
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: model.checking ? "octagon" : "refresh"
                    tip: model.checking ? Strings.sourceStop : Strings.sourceCheck
                    hoverColor: model.checking ? Theme.err : Theme.accent
                    active: !model.fetching && model.total > 0
                    onClicked: model.checking ? ProxyManager.stopSourceCheck(model.sourceId)
                                              : ProxyManager.checkSource(model.sourceId)
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "edit"
                    tip: Strings.editSource
                    onClicked: card.editRequested({ sourceId: model.sourceId, name: model.name,
                                                    url: model.url, defaultType: model.defaultType })
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "trash"
                    tip: Strings.proxyDelete
                    hoverColor: Theme.danger
                    active: model.inUseClients === 0
                    onClicked: card.deleteRequested(model.sourceId, model.name)
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.border
        }

        // Счётчики адресов и занятость в игре.
        Flow {
            width: parent.width
            spacing: Theme.s8

            Chip { text: Strings.sourceTotal(model.total) }
            Chip { text: Strings.sourceOk(model.okCount); tone: model.okCount > 0 ? Theme.ok : Theme.muted }
            Chip { text: Strings.sourceBad(model.badCount); tone: model.badCount > 0 ? Theme.err : Theme.muted }
            Chip { text: Strings.sourceUnchecked(model.uncheckedCount); visible: model.uncheckedCount > 0 }
            Chip {
                visible: model.inUseClients > 0
                text: Strings.sourceInUse(model.inUseClients, model.inUseProxies)
                tone: Theme.accent
            }
        }

        // Ход проверки.
        Column {
            width: parent.width
            spacing: 4
            visible: model.checking

            AppText {
                text: Strings.sourceProgress(model.checkDone, model.checkTotal)
                role: "caption"
                color: Theme.muted
            }
            Rectangle {
                width: parent.width
                height: 6
                radius: 3
                color: Theme.raised
                Rectangle {
                    height: parent.height
                    radius: 3
                    width: parent.width * (model.checkTotal > 0 ? model.checkDone / model.checkTotal : 0)
                    color: Theme.accent
                    Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                }
            }
        }

        // Итог последней операции и дата списка.
        Item {
            width: parent.width
            height: Math.max(statusText.implicitHeight, fetchedText.implicitHeight)
            visible: statusText.text.length > 0 || fetchedText.text.length > 0

            AppText {
                id: statusText
                anchors.left: parent.left
                anchors.right: fetchedText.left
                anchors.rightMargin: Theme.s12
                text: model.fetching ? Strings.sourceFetching : model.status
                role: "caption"
                color: model.statusError ? Theme.err : Theme.muted
                elide: Text.ElideRight
            }
            AppText {
                id: fetchedText
                anchors.right: parent.right
                text: model.fetchedAt.length > 0 ? Strings.sourceFetchedAt(model.fetchedAt) : ""
                role: "caption"
                color: Theme.muted
            }
        }
    }
}
