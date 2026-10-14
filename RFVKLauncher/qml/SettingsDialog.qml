import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Настройки лаунчера: внешний вид и автоподключение улучшенного HUD.
Popup {
    id: dlg
    modal: true
    anchors.centerIn: Overlay.overlay
    padding: Theme.s24
    width: 460
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // Библиотеку могли положить/убрать, пока лаунчер открыт — перепроверяем.
    onAboutToShow: App.refreshHudStatus()

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    background: Rectangle {
        radius: Theme.rDialog
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    contentItem: Column {
        spacing: Theme.s20

        // Заголовок + крестик.
        Item {
            width: parent.width
            height: titleText.height
            AppText { id: titleText; text: Strings.settingsTitle; role: "h2" }
            Rectangle {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                width: 30; height: 30; radius: Theme.rField
                color: xHover.hovered ? Theme.raised : "transparent"
                Icon { anchors.centerIn: parent; name: "cross"; color: Theme.muted; size: 16 }
                HoverHandler { id: xHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: dlg.close() }
            }
        }

        // ── Тема ──
        Row {
            width: parent.width
            spacing: Theme.s12
            Column {
                width: parent.width - themeToggle.width - Theme.s12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                AppText { text: Strings.settingsThemeTitle; role: "bodyStrong" }
                AppText { width: parent.width; text: Strings.settingsThemeDesc; role: "small"; color: Theme.muted; wrapMode: Text.WordWrap }
            }
            AppToggle {
                id: themeToggle
                anchors.verticalCenter: parent.verticalCenter
                checked: App.theme === "dark"
                onToggled: App.setTheme(value ? "dark" : "light")
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        // ── Улучшенный HUD ──
        Column {
            width: parent.width
            spacing: Theme.s12

            Row {
                width: parent.width
                spacing: Theme.s12
                Column {
                    width: parent.width - hudToggle.width - Theme.s12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    AppText { text: Strings.settingsHudTitle; role: "bodyStrong" }
                    AppText { width: parent.width; text: Strings.settingsHudDesc; role: "small"; color: Theme.muted; wrapMode: Text.WordWrap }
                }
                AppToggle {
                    id: hudToggle
                    anchors.verticalCenter: parent.verticalCenter
                    checked: App.hudEnabled
                    onToggled: App.setHudEnabled(value)
                }
            }

            // Не найдена библиотека (или, реже, инжектор из поставки) — подключать нечего.
            Rectangle {
                width: parent.width
                visible: App.hudEnabled && !App.hudAvailable
                radius: Theme.rField
                color: Theme.badgeFill(Theme.warn)
                border.width: 1; border.color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.4)
                implicitHeight: hudCol.implicitHeight + 2 * Theme.s12
                Icon {
                    id: hudIcon
                    anchors.left: parent.left; anchors.leftMargin: Theme.s12
                    anchors.top: parent.top; anchors.topMargin: Theme.s12
                    name: "warn"; color: Theme.warn; size: 16
                }
                Column {
                    id: hudCol
                    anchors.left: hudIcon.right; anchors.leftMargin: Theme.s8
                    anchors.right: parent.right; anchors.rightMargin: Theme.s12
                    anchors.top: parent.top; anchors.topMargin: Theme.s12
                    spacing: 2
                    AppText {
                        text: App.hudDllFound ? Strings.settingsHudNoInjectorTitle : Strings.settingsHudMissingTitle
                        role: "small"; font.weight: Font.DemiBold; color: Theme.warn
                    }
                    AppText {
                        width: parent.width
                        text: App.hudDllFound ? Strings.settingsHudNoInjectorBody : Strings.settingsHudMissingBody
                        role: "small"; color: Theme.muted; wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
