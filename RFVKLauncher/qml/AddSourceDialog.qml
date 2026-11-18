import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Диалог добавления и редактирования источника прокси (список по URL).
Popup {
    id: dlg
    property bool isEdit: false
    property string editSourceId: ""
    property string errorText: ""

    modal: true
    anchors.centerIn: Overlay.overlay
    padding: Theme.s24
    width: 520
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    background: Rectangle {
        radius: Theme.rDialog
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    function openAdd() {
        dlg.isEdit = false
        dlg.editSourceId = ""
        dlg.errorText = ""
        nameField.text = ""
        urlField.text = ""
        typeSelect.currentIndex = 0
        dlg.open()
        urlField.forceActiveFocus()
    }

    function openEdit(src) {
        dlg.isEdit = true
        dlg.editSourceId = src.sourceId
        dlg.errorText = ""
        nameField.text = src.name
        urlField.text = src.url
        typeSelect.currentIndex = (src.defaultType === 2 ? 1 : 0)
        dlg.open()
    }

    // Протокол по ссылке, пока пользователь не выбрал его сам.
    function guessType() {
        var u = urlField.text.toLowerCase()
        if (u.indexOf("socks5") >= 0) typeSelect.currentIndex = 0
        else if (u.indexOf("http/") >= 0 || u.indexOf("/http") >= 0) typeSelect.currentIndex = 1
    }

    function save() {
        var url = urlField.text.trim()
        if (!/^https?:\/\/\S+$/i.test(url)) {
            dlg.errorText = Strings.sourceBadUrl
            return
        }
        // Ошибку проверки на стороне ProxyManager покажет тост; диалог остаётся открытым.
        var ok = dlg.isEdit
            ? ProxyManager.updateSource(dlg.editSourceId, nameField.text, url, typeSelect.currentValue)
            : ProxyManager.addSource(nameField.text, url, typeSelect.currentValue) !== ""
        if (ok)
            dlg.close()
    }

    contentItem: Column {
        spacing: Theme.s16
        width: parent.width

        Item {
            width: parent.width
            height: titleText.height
            AppText {
                id: titleText
                text: dlg.isEdit ? Strings.editSource : Strings.addSource
                role: "h2"
            }
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 30; height: 30; radius: Theme.rField
                color: xHover.hovered ? Theme.raised : "transparent"
                Icon { anchors.centerIn: parent; name: "cross"; color: Theme.muted; size: 16 }
                HoverHandler { id: xHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: dlg.close() }
            }
        }

        Column {
            width: parent.width
            spacing: 4
            AppText { text: Strings.sourceUrl; role: "caption"; color: Theme.muted }
            AppField {
                id: urlField
                width: parent.width
                placeholderText: Strings.sourceUrlHint
                onTextEdited: { dlg.errorText = ""; if (!dlg.isEdit) dlg.guessType() }
            }
            AppText {
                text: Strings.sourceUrlDesc
                role: "caption"
                color: Theme.muted
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        RowLayout {
            width: parent.width
            spacing: Theme.s12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: 4
                AppText { text: Strings.proxyName; role: "caption"; color: Theme.muted }
                AppField {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: Strings.sourceNameHint
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: 4
                AppText { text: Strings.sourceDefaultType; role: "caption"; color: Theme.muted }
                AppSelect {
                    id: typeSelect
                    Layout.fillWidth: true
                    model: [
                        { label: "SOCKS5", value: 1 },
                        { label: "HTTP CONNECT", value: 2 }
                    ]
                }
            }
        }

        AppText {
            visible: dlg.errorText.length > 0
            text: dlg.errorText
            role: "small"
            color: Theme.err
            width: parent.width
            wrapMode: Text.WordWrap
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        Item {
            width: parent.width
            height: Theme.hField

            Row {
                anchors.right: parent.right
                spacing: Theme.s12

                SecondaryButton {
                    kind: "ghost"
                    text: Strings.cancel
                    onClicked: dlg.close()
                }
                PrimaryButton {
                    text: dlg.isEdit ? Strings.proxySave : Strings.sourceAdd
                    iconName: "check"
                    onClicked: dlg.save()
                }
            }
        }
    }
}
