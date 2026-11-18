import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Главное окно: верхняя полоса с переключением вкладок («Аккаунты» / «Прокси и слоты»),
// рабочая область и строка состояния.
Window {
    id: win
    width: 960
    height: Math.min(760, Screen.desktopAvailableHeight - 40)
    minimumWidth: 720
    minimumHeight: 640
    visible: true
    title: Strings.appTitle
    color: Theme.bg

    // Вкладка навигации: "accounts" или "proxies"
    property string activeTab: "accounts"

    // Узкое окно -> компактный сайдбар (только аватар + точка статуса).
    readonly property bool compact: width < 860
    readonly property bool hasSelection: App.selectedCount > 0

    // Переключение темы: Ctrl+T.
    Shortcut {
        sequence: "Ctrl+T"
        onActivated: App.setTheme(App.theme === "dark" ? "light" : "dark")
    }
    // Выделить все аккаунты: Ctrl+A.
    Shortcut {
        sequence: StandardKey.SelectAll
        onActivated: {
            if (win.activeTab === "accounts")
                App.selectAll()
        }
    }
    // Быстрое переключение вкладок: Ctrl+1 (Аккаунты), Ctrl+2 (Прокси)
    Shortcut {
        sequence: "Ctrl+1"
        onActivated: win.activeTab = "accounts"
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: win.activeTab = "proxies"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Верхняя полоса ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.surface
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

            // Левая часть: логотип + название + пилюля бренда
            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.s20
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s12

                // Логотип-октагон (двухтоновая эмблема).
                Item {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26; height: 26
                    Icon { anchors.centerIn: parent; name: "octagon"; color: Theme.accent; size: 26 }
                    Icon { anchors.centerIn: parent; name: "octagon"; color: Theme.surface; size: 15 }
                    Rectangle { anchors.centerIn: parent; width: 6; height: 6; radius: 3; color: Theme.accent }
                }
                AppText { anchors.verticalCenter: parent.verticalCenter; text: Strings.appTitle; role: "h3" }

                // Пилюля бренда
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24; width: bp.implicitWidth + 2 * Theme.s12; radius: Theme.rBadge
                    color: Theme.raised; border.width: 1; border.color: Theme.border
                    AppText { id: bp; anchors.centerIn: parent; text: Strings.brandPill; role: "caption"; color: Theme.muted }
                }
            }

            // Центр: Сегментированный переключатель вкладок («Аккаунты» / «Прокси и слоты»)
            Rectangle {
                anchors.centerIn: parent
                height: 32
                width: tabRow.implicitWidth + 8
                radius: Theme.rField
                color: Theme.raised
                border.width: 1
                border.color: Theme.border

                Row {
                    id: tabRow
                    anchors.centerIn: parent
                    spacing: 4

                    // Вкладка «Аккаунты»
                    Rectangle {
                        height: 26
                        width: tabAccText.implicitWidth + 24
                        radius: Theme.rBadge
                        color: win.activeTab === "accounts" ? Theme.surface : "transparent"
                        border.width: win.activeTab === "accounts" ? 1 : 0
                        border.color: Theme.border

                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "userplus"
                                size: 14
                                color: win.activeTab === "accounts" ? Theme.accent : Theme.muted
                            }
                            AppText {
                                id: tabAccText
                                anchors.verticalCenter: parent.verticalCenter
                                text: Strings.tabAccounts
                                role: "caption"
                                font.weight: win.activeTab === "accounts" ? Font.DemiBold : Font.Normal
                                color: win.activeTab === "accounts" ? Theme.text : Theme.muted
                            }
                        }

                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: win.activeTab = "accounts" }
                    }

                    // Вкладка «Прокси и слоты»
                    Rectangle {
                        height: 26
                        width: tabProxText.implicitWidth + (ProxyManager.totalActiveClients > 0 ? cntBadge.width + 30 : 24)
                        radius: Theme.rBadge
                        color: win.activeTab === "proxies" ? Theme.surface : "transparent"
                        border.width: win.activeTab === "proxies" ? 1 : 0
                        border.color: Theme.border

                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "globe"
                                size: 14
                                color: win.activeTab === "proxies" ? Theme.accent : Theme.muted
                            }
                            AppText {
                                id: tabProxText
                                anchors.verticalCenter: parent.verticalCenter
                                text: Strings.tabProxies
                                role: "caption"
                                font.weight: win.activeTab === "proxies" ? Font.DemiBold : Font.Normal
                                color: win.activeTab === "proxies" ? Theme.text : Theme.muted
                            }
                            // Живой бейдж запущенных клиентов RF Online
                            Rectangle {
                                id: cntBadge
                                anchors.verticalCenter: parent.verticalCenter
                                visible: ProxyManager.totalActiveClients > 0
                                height: 16
                                width: Math.max(16, cntText.implicitWidth + 8)
                                radius: 8
                                color: Theme.badgeFill(Theme.ok)
                                border.width: 1
                                border.color: Qt.rgba(Theme.ok.r, Theme.ok.g, Theme.ok.b, 0.4)
                                AppText {
                                    id: cntText
                                    anchors.centerIn: parent
                                    text: ProxyManager.totalActiveClients
                                    role: "caption"
                                    font.weight: Font.DemiBold
                                    color: Theme.ok
                                }
                            }
                        }

                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: win.activeTab = "proxies" }
                    }
                }
            }

            // Правая часть: кнопка темы + кнопка настроек
            Row {
                anchors.right: parent.right
                anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s4
                SecondaryButton {
                    height: 32; width: 32; kind: "ghost"
                    iconName: App.theme === "dark" ? "sun" : "moon"
                    onClicked: App.setTheme(App.theme === "dark" ? "light" : "dark")
                }
                SecondaryButton {
                    height: 32; width: 32; kind: "ghost"
                    iconName: "sliders"
                    onClicked: settingsDlg.open()
                }
            }
        }

        // ── Основное содержимое: Вкладки StackLayout ──
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.activeTab === "proxies" ? 1 : 0

            // ── Вкладка 0: Аккаунты (Сайдбар + Панель выбранного аккаунта + Каталог игры + Журнал) ──
            RowLayout {
                spacing: 0

                Sidebar {
                    id: sidebar
                    Layout.fillHeight: true
                    Layout.preferredWidth: win.compact ? 248 : Theme.sidebarW
                    compact: win.compact
                    onAddRequested: addDlg.open()
                }

                // Правая зона аккаунтов
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.s24
                        anchors.rightMargin: Theme.s24
                        anchors.topMargin: Theme.s20
                        anchors.bottomMargin: Theme.s20
                        spacing: Theme.s16

                        // Панель аккаунта, сводка по нескольким выделенным или пустое состояние
                        Loader {
                            Layout.fillWidth: true
                            sourceComponent: !win.hasSelection ? emptyComp
                                           : App.selectedCount > 1 ? multiComp : panelComp
                        }

                        GameDirRow { Layout.fillWidth: true }

                        LogConsole {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 120
                        }
                    }
                }
            }

            // ── Вкладка 1: Прокси и слоты маршрутизации ──
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s24
                    anchors.rightMargin: Theme.s24
                    anchors.topMargin: Theme.s20
                    anchors.bottomMargin: Theme.s20
                    spacing: Theme.s16

                    ProxyPanel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onAddRequested: addProxyDlg.openAdd()
                        onEditRequested: (slot) => addProxyDlg.openEdit(slot)
                        onDeleteRequested: (slotId, slotName) => {
                            delProxyConfirmDlg.slotId = slotId
                            delProxyConfirmDlg.slotName = slotName
                            delProxyConfirmDlg.open()
                        }
                    }

                    LogConsole {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        Layout.minimumHeight: 100
                    }
                }
            }
        }

        // ── Нижняя строка состояния ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Theme.surface
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }

            Row {
                anchors.left: parent.left; anchors.leftMargin: Theme.s16
                anchors.right: statusRightRow.left; anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8; height: 8; radius: 4
                    color: App.errorText.length ? Theme.err : App.busy ? Theme.accent : App.accountCount === 0 ? Theme.muted : Theme.ok
                }
                AppText {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 16
                    elide: Text.ElideRight
                    role: "small"; color: Theme.muted
                    text: App.errorText.length ? (Strings.errorPrefix + " " + App.errorText)
                        : App.busy ? (Strings.working + " " + App.progressStep)
                        : App.accountCount === 0 ? Strings.noAccounts
                        : Strings.ready
                }
            }

            Row {
                id: statusRightRow
                anchors.right: parent.right; anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s16

                // Индикатор работающих окон через слоты
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.s8
                    visible: ProxyManager.totalActiveClients > 0
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8; height: 8; radius: 4
                        color: Theme.ok
                    }
                    AppText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Strings.proxyActiveWindows(ProxyManager.totalActiveClients)
                        role: "small"
                        color: Theme.text
                    }
                }

                // Индикатор GameCenter
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.s8
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8; height: 8; radius: 4
                        color: App.gameCenterFound ? Theme.ok : Theme.muted
                    }
                    AppText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: App.gameCenterFound ? Strings.gcFound : Strings.gcNotFound
                        role: "small"
                        color: Theme.muted
                    }
                }
            }
        }
    }

    Component {
        id: panelComp
        AccountPanel {
            onDeleteRequested: confirmDelete()
        }
    }
    Component {
        id: multiComp
        MultiPanel {
            onDeleteRequested: confirmDelete()
        }
    }
    Component {
        id: emptyComp
        EmptyState {
            onImportRequested: App.importSession()
        }
    }

    // Подтверждение удаления всех выделенных аккаунтов.
    function confirmDelete() {
        var list = App.selectedAccounts
        if (!list.length) return
        confirmDlg.count = list.length
        confirmDlg.accName = list[0].nick
        confirmDlg.accKind = list[0].loginKind
        confirmDlg.accLogin = list[0].login
        confirmDlg.names = list.map(function (a) { return a.nick }).join(", ")
        confirmDlg.open()
    }

    AddAccountDialog { id: addDlg }
    SettingsDialog { id: settingsDlg }
    AddProxyDialog { id: addProxyDlg }

    // Диалог подтверждения удаления прокси
    Popup {
        id: delProxyConfirmDlg
        property string slotId: ""
        property string slotName: ""

        modal: true
        anchors.centerIn: Overlay.overlay
        padding: Theme.s24
        width: 400
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

        background: Rectangle {
            radius: Theme.rDialog
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
        }

        contentItem: Column {
            spacing: Theme.s16

            Rectangle {
                width: 44; height: 44; radius: Theme.rPill
                color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14)
                Icon { anchors.centerIn: parent; name: "trash"; color: Theme.danger; size: 20 }
            }

            AppText {
                text: Strings.proxyDeleteConfirmTitle
                role: "h2"; width: parent.width; wrapMode: Text.WordWrap
            }
            AppText {
                width: parent.width; wrapMode: Text.WordWrap
                role: "small"; color: Theme.muted
                text: Strings.proxyDeleteConfirmBody
            }

            Item {
                width: parent.width
                height: Theme.hField

                Row {
                    anchors.right: parent.right
                    spacing: Theme.s12

                    SecondaryButton {
                        kind: "ghost"
                        text: Strings.cancel
                        onClicked: delProxyConfirmDlg.close()
                    }
                    SecondaryButton {
                        kind: "danger"
                        text: Strings.proxyDelete
                        fontRole: "bodyStrong"
                        onClicked: {
                            ProxyManager.removeProxy(delProxyConfirmDlg.slotId)
                            delProxyConfirmDlg.close()
                        }
                    }
                }
            }
        }
    }

    // Автодействия для скриншотов (RFVK_OPEN=add|launch|refresh|proxies|addproxy).
    Component.onCompleted: {
        if (typeof StartupOpen === "undefined") return
        if (StartupOpen === "add") addDlg.open()
        else if (StartupOpen === "settings") settingsDlg.open()
        else if (StartupOpen === "proxies") win.activeTab = "proxies"
        else if (StartupOpen === "addproxy") { win.activeTab = "proxies"; addProxyDlg.openAdd() }
        else if (StartupOpen === "addproxyauth") { win.activeTab = "proxies"; addProxyDlg.openAdd(true) }
        else if (StartupOpen === "delete") confirmDelete()
        else if (StartupOpen === "deleteconfirm") App.deleteSelected()
        else if (StartupOpen === "import") App.importSession()
        else if (StartupOpen === "launch") App.launchSelected()
        else if (StartupOpen === "refresh") App.refreshSelected()
    }

    ConfirmDialog {
        id: confirmDlg
        onConfirmed: App.deleteSelected()
    }

    Toasts {}
}
