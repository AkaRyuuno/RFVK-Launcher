import QtQuick
import Rfvk

// Правая зона: панель выбранного аккаунта — крупный заголовок с действиями,
// карточка состояния сессии и главная кнопка запуска.
Item {
    id: panel
    property var acc: App.selectedAccount
    signal deleteRequested()

    readonly property color avColor: Theme.avatarColor(acc && acc.id ? acc.id : "")
    readonly property color stColor: Theme.statusColor(acc ? acc.sessionState : App.None)

    // Инициалы как в модели: по двум словам или первые две буквы.
    function initialsOf(nick) {
        var s = (nick || "").trim()
        if (!s.length) return "?"
        var parts = s.split(/[\s_]+/).filter(function (x) { return x.length })
        if (parts.length >= 2) return (parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase()
        return s.substring(0, 2).toUpperCase()
    }

    implicitHeight: content.implicitHeight

    // Русские названия месяцев (locale "d MMM" даёт «16 окт»).
    function fmt(d) { return d.toLocaleString(Qt.locale("ru_RU"), "d MMM, HH:mm") }
    function ago(d) {
        if (!d || isNaN(d.getTime())) return ""
        var m = Math.round((Date.now() - d.getTime()) / 60000)
        if (m < 1) return "только что"
        if (m < 60) return m + " мин назад"
        var h = Math.round(m / 60)
        if (h < 24) return h + " ч назад"
        return Math.round(h / 24) + " дн назад"
    }
    function until(d) {
        if (!d || isNaN(d.getTime())) return ""
        var m = Math.round((d.getTime() - Date.now()) / 60000)
        if (m <= 0) return "уже истекла"
        if (m < 60) return "через " + m + " мин"
        var h = Math.round(m / 60)
        if (h < 48) return "примерно через " + h + " ч"
        return "через " + Math.round(h / 24) + " дн"
    }
    function subtitle() {
        if (!acc || !acc.valid) return ""
        switch (acc.sessionState) {
        case App.Active:
            var upd = ago(acc.sessionRefreshed)
            return "Действует до " + fmt(acc.sessionExpiry) + (upd.length ? " · обновлена " + upd : "")
        case App.Expiring: return "Истекает " + until(acc.sessionExpiry) + " — обновите заранее"
        case App.Expired: return "Истекла " + fmt(acc.sessionExpiry) + " — нужен импорт из GameCenter"
        default: return "Сессия не сохранена — нужен импорт из GameCenter"
        }
    }

    Column {
        id: content
        width: parent.width
        spacing: Theme.s16

        // ── Заголовок: аватар + ник/логин + действия справа ──
        Item {
            width: parent.width
            implicitHeight: Math.max(hdrLeft.height, hdrRight.height)

            Row {
                id: hdrLeft
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - hdrRight.width - Theme.s16
                spacing: Theme.s16
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 56; height: 56; radius: Theme.rDialog
                    color: Theme.avatarFill(panel.avColor)
                    AppText {
                        anchors.centerIn: parent
                        text: panel.initialsOf(panel.acc ? panel.acc.nick : "")
                        role: "h2"; color: panel.avColor
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 56 - Theme.s16
                    spacing: Theme.s4
                    AppText { text: panel.acc ? panel.acc.nick : ""; role: "display"; elide: Text.ElideRight; width: parent.width }
                    Row {
                        spacing: Theme.s8
                        AppText {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Strings.loginKindLabel(panel.acc ? panel.acc.loginKind : "") + " · " + (panel.acc ? panel.acc.login : "")
                            role: "small"; color: Theme.muted
                        }
                        // Пилюля «Вход: <способ>».
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 20; width: lp.implicitWidth + 2 * Theme.s8; radius: Theme.rBadge
                            color: Theme.raised; border.width: 1; border.color: Theme.border
                            AppText {
                                id: lp; anchors.centerIn: parent
                                text: Strings.loginPrefix + " " + Strings.loginKindLabel(panel.acc ? panel.acc.loginKind : "")
                                role: "caption"; color: Theme.muted
                            }
                        }
                    }
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
                // Удаление — компактная кнопка-иконка.
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

        // ── Карточка состояния сессии (подкрашена цветом статуса) ──
        Rectangle {
            width: parent.width
            radius: Theme.rCard
            color: Theme.badgeFill(panel.stColor)
            border.width: 1; border.color: Qt.rgba(panel.stColor.r, panel.stColor.g, panel.stColor.b, 0.35)
            implicitHeight: Math.max(stCol.implicitHeight, fixRow.visible ? fixRow.height : 0) + 2 * Theme.s16

            Row {
                id: stRow
                anchors.left: parent.left; anchors.leftMargin: Theme.s16
                anchors.top: parent.top; anchors.topMargin: Theme.s16
                width: parent.width - 2 * Theme.s16 - (fixRow.visible ? fixRow.width + Theme.s12 : 0)
                spacing: Theme.s12
                Rectangle {
                    anchors.top: parent.top; anchors.topMargin: 5
                    width: 10; height: 10; radius: 5; color: panel.stColor
                }
                Column {
                    id: stCol
                    width: parent.width - 10 - Theme.s12
                    spacing: 2
                    AppText { text: Strings.statusTitle(panel.acc ? panel.acc.sessionState : App.None); role: "bodyStrong"; color: panel.stColor }
                    AppText { width: parent.width; text: panel.subtitle(); role: "small"; color: Theme.muted; wrapMode: Text.WordWrap }
                }
            }

            // Быстрые действия при истёкшей/отсутствующей сессии.
            Row {
                id: fixRow
                anchors.right: parent.right; anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8
                visible: panel.acc && (panel.acc.sessionState === App.Expired || panel.acc.sessionState === App.None)
                SecondaryButton {
                    height: 36; kind: "accent"; iconName: "download"; text: Strings.quickImport; fontRole: "small"
                    enabled: !App.busy
                    onClicked: App.importSession()
                }
            }
        }

        // ── Баннер ошибки (после неудачной операции) ──
        Rectangle {
            width: parent.width
            visible: App.errorText.length > 0
            radius: Theme.rCard
            color: Theme.badgeFill(Theme.err)
            border.width: 1; border.color: Qt.rgba(Theme.err.r, Theme.err.g, Theme.err.b, 0.4)
            implicitHeight: errCol.implicitHeight + 2 * Theme.s16
            Icon {
                id: errIcon
                anchors.left: parent.left; anchors.leftMargin: Theme.s16
                anchors.top: parent.top; anchors.topMargin: Theme.s16
                name: "warn"; color: Theme.err; size: 18
            }
            Column {
                id: errCol
                anchors.left: errIcon.right; anchors.leftMargin: Theme.s12
                anchors.top: parent.top; anchors.topMargin: Theme.s16
                anchors.right: errClose.left; anchors.rightMargin: Theme.s12
                spacing: 2
                AppText { text: Strings.errorBannerTitle; role: "bodyStrong"; color: Theme.err }
                AppText { width: parent.width; text: App.errorText; role: "small"; color: Theme.muted; wrapMode: Text.WordWrap }
            }
            Rectangle {
                id: errClose
                anchors.right: parent.right; anchors.rightMargin: Theme.s8
                anchors.top: parent.top; anchors.topMargin: Theme.s8
                width: 28; height: 28; radius: Theme.rField
                color: ecHover.hovered ? Theme.raised : "transparent"
                Icon { anchors.centerIn: parent; name: "cross"; color: Theme.muted; size: 14 }
                HoverHandler { id: ecHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: App.dismissError() }
            }
        }

        // ── Автовход: FG-пароль и персонаж ──
        AutoLoginCard {
            width: parent.width
            acc: panel.acc
        }

        // ── Главная кнопка (или «Повторить запуск» после ошибки) ──
        PrimaryButton {
            width: parent.width
            retry: App.errorText.length > 0 && !App.busy
            text: retry ? Strings.launchRetry : Strings.launch
            iconName: retry ? "refresh" : "play"
            busy: App.busy
            busyText: App.progressStep
            counter: App.progressTotal > 0 ? (App.progressIndex + "/" + App.progressTotal) : ""
            onClicked: App.launchSelected()
        }
    }
}
