import QtQuick
import QtQuick.Controls.Basic
import Rfvk

// Диалог подтверждения удаления аккаунта (одного или нескольких выделенных).
Popup {
    id: dlg
    property string accName: ""
    property string accKind: ""
    property string accLogin: ""
    property int count: 1          // сколько аккаунтов удаляем
    property string names: ""      // ники через запятую (для нескольких)
    signal confirmed(bool revokeSession)

    modal: true
    anchors.centerIn: Overlay.overlay
    padding: Theme.s24
    width: 400
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onAboutToShow: revokeToggle.checked = false

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    background: Rectangle {
        radius: Theme.rDialog
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    contentItem: Column {
        spacing: Theme.s16

        // Иконка-предупреждение.
        Rectangle {
            width: 44; height: 44; radius: Theme.rPill
            color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14)
            Icon { anchors.centerIn: parent; name: "trash"; color: Theme.danger; size: 20 }
        }

        AppText {
            text: dlg.count > 1 ? "Удалить " + Strings.accountsN(dlg.count) + "?"
                                : "Удалить аккаунт «" + dlg.accName + "»?"
            role: "h2"; width: parent.width; wrapMode: Text.WordWrap
        }
        AppText {
            width: parent.width
            wrapMode: Text.WordWrap
            role: "small"
            color: Theme.muted
            text: dlg.count > 1 ? "Из лаунчера будут удалены записи: " + dlg.names
                                : "Из лаунчера будет удалена запись для "
                                  + Strings.loginKindLabel(dlg.accKind) + " · " + dlg.accLogin + "."
        }

        Column {
            width: parent.width
            spacing: Theme.s4
            CheckBox {
                id: revokeToggle
                text: Strings.revokeDeletedSession
                palette.windowText: Theme.text
                palette.highlight: Theme.accent
            }
            AppText {
                width: parent.width
                text: Strings.revokeDeletedSessionHint
                role: "small"
                color: Theme.muted
                wrapMode: Text.WordWrap
            }
        }

        // Кнопки — обе справа.
        Item {
            width: parent.width
            height: Theme.hField
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s8
                SecondaryButton { kind: "raised"; text: Strings.cancel; onClicked: dlg.close() }
                SecondaryButton {
                    kind: "danger"; iconName: "trash"; text: Strings.confirmDeleteYes
                    onClicked: { dlg.confirmed(revokeToggle.checked); dlg.close() }
                }
            }
        }
    }
}
