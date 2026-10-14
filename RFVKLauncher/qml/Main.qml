import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Главное окно: верхняя полоса + две зоны (сайдбар аккаунтов слева, панель
// выбранного аккаунта + каталог игры + журнал справа).
Window {
    id: win
    width: 960
    height: 620
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: Strings.appTitle
    color: Theme.bg

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
        onActivated: App.selectAll()
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
                // Пилюля бренда.
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24; width: bp.implicitWidth + 2 * Theme.s12; radius: Theme.rBadge
                    color: Theme.raised; border.width: 1; border.color: Theme.border
                    AppText { id: bp; anchors.centerIn: parent; text: Strings.brandPill; role: "caption"; color: Theme.muted }
                }
            }

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

        // ── Две зоны ──
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                id: sidebar
                Layout.fillHeight: true
                Layout.preferredWidth: win.compact ? 248 : Theme.sidebarW
                compact: win.compact
                onAddRequested: addDlg.open()
            }

            // Правая зона.
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

                    // Панель аккаунта, сводка по нескольким выделенным или пустое состояние.
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

        // ── Нижняя строка состояния ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Theme.surface
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
            Row {
                anchors.left: parent.left; anchors.leftMargin: Theme.s16
                anchors.right: gcRow.left; anchors.rightMargin: Theme.s16
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
                id: gcRow
                anchors.right: parent.right; anchors.rightMargin: Theme.s16
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 8; height: 8; radius: 4; color: App.gameCenterFound ? Theme.ok : Theme.muted }
                AppText { anchors.verticalCenter: parent.verticalCenter; text: App.gameCenterFound ? Strings.gcFound : Strings.gcNotFound; role: "small"; color: Theme.muted }
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

    // Автодействия для скриншотов (RFVK_OPEN=add|launch|refresh).
    Component.onCompleted: {
        if (typeof StartupOpen === "undefined") return
        if (StartupOpen === "add") addDlg.open()
        else if (StartupOpen === "settings") settingsDlg.open()
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
