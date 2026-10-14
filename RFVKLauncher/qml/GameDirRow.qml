import QtQuick
import QtQuick.Dialogs
import Rfvk

// Строка «Каталог игры»: поле с путём (иконка папки + путь + чип статуса
// RF_Online.exe) и кнопка «Обзор…». Значение сохраняется контроллером.
Column {
    id: root
    spacing: Theme.s8

    function urlToPath(u) {
        var s = "" + u
        s = s.replace(/^file:\/\/\//, "")
        return decodeURIComponent(s)
    }

    AppText { text: Strings.gameDir; role: "overline"; color: Theme.muted }

    Row {
        width: parent.width
        spacing: Theme.s8

        Rectangle {
            id: pathBox
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - browseBtn.width - Theme.s8
            height: Theme.hField
            radius: Theme.rField
            color: Theme.raised
            border.width: 1
            border.color: Theme.border

            Icon {
                id: folderIcon
                anchors.left: parent.left; anchors.leftMargin: Theme.s12
                anchors.verticalCenter: parent.verticalCenter
                name: "folder"; color: Theme.muted; size: 16
            }
            AppText {
                anchors.left: folderIcon.right; anchors.leftMargin: Theme.s8
                anchors.right: chip.left; anchors.rightMargin: Theme.s8
                anchors.verticalCenter: parent.verticalCenter
                text: App.gameDir.length ? App.gameDir : Strings.gameDirEmpty
                role: "body"
                color: App.gameDir.length ? Theme.text : Theme.muted
                elide: Text.ElideMiddle
            }
            // Чип статуса RF_Online.exe — внутри поля справа.
            Rectangle {
                id: chip
                anchors.right: parent.right; anchors.rightMargin: Theme.s8
                anchors.verticalCenter: parent.verticalCenter
                visible: App.gameDir.length > 0
                height: 24; width: chipRow.implicitWidth + 2 * Theme.s8; radius: Theme.rBadge
                readonly property color c: App.gameDirValid ? Theme.ok : Theme.err
                color: Theme.badgeFill(c)
                Row {
                    id: chipRow
                    anchors.centerIn: parent
                    spacing: Theme.s4
                    Icon { anchors.verticalCenter: parent.verticalCenter; name: App.gameDirValid ? "check" : "warn"; color: chip.c; size: 13 }
                    AppText { anchors.verticalCenter: parent.verticalCenter; text: App.gameDirValid ? Strings.rfFound : Strings.rfNotFound; role: "caption"; font.weight: Font.DemiBold; color: chip.c }
                }
            }
        }
        SecondaryButton {
            id: browseBtn
            anchors.verticalCenter: parent.verticalCenter
            kind: "raised"
            iconName: "folder"
            text: Strings.browse
            onClicked: folderDialog.open()
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Выберите папку RF Online"
        onAccepted: App.setGameDir(root.urlToPath(selectedFolder))
    }
}
