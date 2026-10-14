import QtQuick
import Rfvk

// Онбординг правой зоны, когда аккаунтов ещё нет: заголовок, подзаголовок и
// карточка импорта сессии из GameCenter.
Item {
    id: empty
    signal importRequested()

    implicitHeight: col.implicitHeight

    Column {
        id: col
        width: parent.width
        spacing: Theme.s12

        AppText { text: Strings.emptyBigTitle; role: "display" }
        AppText {
            width: parent.width
            text: Strings.emptyBigSubtitle
            role: "body"; color: Theme.muted; wrapMode: Text.WordWrap
        }

        // Карточка импорта.
        Rectangle {
            width: parent.width
            radius: Theme.rCard
            color: Theme.badgeFill(Theme.accent)
            border.width: 1; border.color: Theme.accent
            implicitHeight: imCol.implicitHeight + 2 * Theme.s16
            Column {
                id: imCol
                x: Theme.s16; y: Theme.s16
                width: parent.width - 2 * Theme.s16
                spacing: Theme.s8
                Row {
                    spacing: Theme.s12
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32; height: 32; radius: Theme.rBadge; color: Theme.accent
                        Icon { anchors.centerIn: parent; name: "download"; color: Theme.accentInk; size: 18 }
                    }
                    AppText { anchors.verticalCenter: parent.verticalCenter; text: Strings.methodImportTitle; role: "h3" }
                }
                AppText { width: parent.width; text: Strings.methodImportDesc; role: "small"; color: Theme.muted; wrapMode: Text.WordWrap }
                Item { width: 1; height: Theme.s4 }
                SecondaryButton {
                    kind: "accent"; iconName: "download"; text: Strings.importNow; fontRole: "bodyStrong"
                    enabled: !App.busy
                    onClicked: empty.importRequested()
                }
            }
        }
    }
}
