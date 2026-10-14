import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Rfvk

// Диалог «Добавить аккаунт»: импорт текущей сессии из GameCenter (без пароля).
Popup {
    id: dlg
    modal: true
    anchors.centerIn: Overlay.overlay
    padding: Theme.s24
    width: 460
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    background: Rectangle {
        radius: Theme.rDialog
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    contentItem: Column {
        spacing: Theme.s20

        // Заголовок диалога.
        Item {
            width: parent.width
            height: titleText.height
            AppText { id: titleText; text: Strings.addAccount; role: "h2" }
            Rectangle {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                width: 30; height: 30; radius: Theme.rField
                color: xHover.hovered ? Theme.raised : "transparent"
                Icon { anchors.centerIn: parent; name: "cross"; color: Theme.muted; size: 16 }
                HoverHandler { id: xHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: dlg.close() }
            }
        }

        // ── Импорт сессии (выделенная карточка) ──
        Rectangle {
            width: parent.width
            radius: Theme.rCard
            color: Theme.badgeFill(Theme.accent)
            border.width: 1
            border.color: Theme.accent
            implicitHeight: imp.implicitHeight + 2 * Theme.s16
            Column {
                id: imp
                x: Theme.s16; y: Theme.s16
                width: parent.width - 2 * Theme.s16
                spacing: Theme.s12
                // Заголовок карточки: иконка + название.
                Row {
                    spacing: Theme.s8
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28; height: 28; radius: Theme.rBadge
                        color: Theme.raised
                        Icon { anchors.centerIn: parent; name: "download"; color: Theme.accent; size: 16 }
                    }
                    AppText { anchors.verticalCenter: parent.verticalCenter; text: Strings.importTitle; role: "bodyStrong" }
                }
                AppText { width: parent.width; text: Strings.importHint; role: "small"; color: Theme.muted; wrapMode: Text.WordWrap }

                // Баннер: GameCenter не найден.
                Rectangle {
                    width: parent.width
                    visible: !App.gameCenterFound
                    radius: Theme.rField
                    color: Theme.badgeFill(Theme.warn)
                    border.width: 1; border.color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.4)
                    implicitHeight: gcCol.implicitHeight + 2 * Theme.s12
                    Icon {
                        id: gcIcon
                        anchors.left: parent.left; anchors.leftMargin: Theme.s12
                        anchors.top: parent.top; anchors.topMargin: Theme.s12
                        name: "warn"; color: Theme.warn; size: 16
                    }
                    Column {
                        id: gcCol
                        anchors.left: gcIcon.right; anchors.leftMargin: Theme.s8
                        anchors.right: parent.right; anchors.rightMargin: Theme.s12
                        anchors.top: parent.top; anchors.topMargin: Theme.s12
                        spacing: 2
                        AppText { text: Strings.gcWarnTitle; role: "small"; font.weight: Font.DemiBold; color: Theme.warn }
                        AppText { width: parent.width; text: Strings.gcWarnBody; role: "small"; color: Theme.muted; wrapMode: Text.WordWrap }
                    }
                }

                // Кнопка импорта (+ «Указать путь…», если GameCenter не найден).
                Row {
                    width: parent.width
                    spacing: Theme.s8
                    PrimaryButton {
                        width: App.gameCenterFound ? parent.width : parent.width - pathBtn.width - Theme.s8
                        text: Strings.importButton
                        iconName: "download"
                        wrapText: !App.gameCenterFound
                        busy: App.busy
                        onClicked: { App.importSession(); dlg.close() }
                    }
                    SecondaryButton {
                        id: pathBtn
                        visible: !App.gameCenterFound
                        height: Theme.hMain
                        kind: "raised"
                        text: Strings.specifyPath
                        onClicked: gcFolderDialog.open()
                    }
                }
            }
        }
    }

    // Выбор папки GameCenter вручную (когда он не найден автоматически).
    // TODO: передать путь реальному бэкенду импорта.
    FolderDialog {
        id: gcFolderDialog
        title: "Укажите папку VK Play GameCenter"
    }
}
