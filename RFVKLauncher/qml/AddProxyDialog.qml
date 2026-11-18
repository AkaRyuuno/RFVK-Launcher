import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Диалог добавления и редактирования SOCKS5 / HTTP прокси-серверов.
Popup {
    id: dlg
    property bool isEdit: false
    property string editSlotId: ""
    // Проверка в диалоге: "" — не было, "testing", "ok", "error".
    property string testState: ""
    property string testText: ""

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

    Connections {
        target: ProxyManager
        function onCustomTestFinished(ok, pingMs, message) {
            if (dlg.testState !== "testing")
                return // диалог закрыли или открыли заново
            dlg.testState = ok ? "ok" : "error"
            dlg.testText = ok ? Strings.proxyTestOk(pingMs, message) : Strings.proxyTestFailed(message)
        }
    }

    function reset() {
        dlg.testState = ""
        dlg.testText = ""
        passField.text = ""
    }

    function openAdd(withAuth) {
        dlg.isEdit = false
        dlg.editSlotId = ""
        nameField.text = ""
        typeSelect.currentIndex = 0 // SOCKS5
        hostField.text = ""
        portField.text = "1080"
        authToggle.checked = !!withAuth
        userField.text = ""
        reset()
        dlg.open()
    }

    // Пароль в интерфейс не отдаётся: пустое поле при сохранении — оставить прежний.
    function openEdit(slot) {
        dlg.isEdit = true
        dlg.editSlotId = slot.slotId
        nameField.text = slot.name
        typeSelect.currentIndex = (slot.type === 2 ? 1 : 0)
        hostField.text = slot.host
        portField.text = slot.port.toString()
        authToggle.checked = slot.authEnabled
        userField.text = slot.username
        reset()
        dlg.open()
    }

    function fail(text) {
        dlg.testState = "error"
        dlg.testText = text
    }

    // Порт из поля или NaN (поле проверяет IntValidator, но может быть пустым).
    function port() {
        var p = parseInt(portField.text.trim(), 10)
        return (isNaN(p) || p <= 0 || p > 65535) ? NaN : p
    }

    function runTest() {
        if (!hostField.text.trim().length || isNaN(port()))
            return fail(Strings.proxyBadHostPort)
        dlg.testState = "testing"
        dlg.testText = Strings.proxyTesting
        ProxyManager.testCustomProxy(typeSelect.currentValue, hostField.text, port(), authToggle.checked,
                                     userField.text, passField.text, dlg.editSlotId)
    }

    // Проверку данных повторяет ProxyManager; при ошибке он покажет тост, и диалог останется открытым.
    function save() {
        if (!hostField.text.trim().length || isNaN(port()))
            return fail(Strings.proxyBadHostPort)
        var ok = dlg.isEdit
            ? ProxyManager.updateProxy(dlg.editSlotId, nameField.text, typeSelect.currentValue, hostField.text,
                                       port(), authToggle.checked, userField.text, passField.text)
            : ProxyManager.addProxy(nameField.text, typeSelect.currentValue, hostField.text, port(),
                                    authToggle.checked, userField.text, passField.text) !== ""
        if (ok)
            dlg.close()
    }

    contentItem: Column {
        spacing: Theme.s16
        width: parent.width

        // Шапка диалога
        Item {
            width: parent.width
            height: titleText.height
            AppText {
                id: titleText
                text: dlg.isEdit ? Strings.editProxy : Strings.addProxy
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

        // Название слота
        Column {
            width: parent.width
            spacing: 4
            AppText { text: Strings.proxyName; role: "caption"; color: Theme.muted }
            AppField {
                id: nameField
                width: parent.width
                placeholderText: Strings.proxyNameHint
            }
        }

        // Протокол + Порт в одну строку
        RowLayout {
            width: parent.width
            spacing: Theme.s12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                AppText { text: Strings.proxyType; role: "caption"; color: Theme.muted }
                AppSelect {
                    id: typeSelect
                    Layout.fillWidth: true
                    model: [
                        { label: "SOCKS5", value: 1 },
                        { label: "HTTP CONNECT", value: 2 }
                    ]
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 110
                spacing: 4
                AppText { text: Strings.proxyPort; role: "caption"; color: Theme.muted }
                AppField {
                    id: portField
                    Layout.fillWidth: true
                    placeholderText: "1080"
                    validator: IntValidator { bottom: 1; top: 65535 }
                }
            }
        }

        // Хост / IP
        Column {
            width: parent.width
            spacing: 4
            AppText { text: Strings.proxyHost; role: "caption"; color: Theme.muted }
            AppField {
                id: hostField
                width: parent.width
                placeholderText: Strings.proxyHostHint
            }
        }

        // Переключатель авторизации
        Row {
            width: parent.width
            spacing: Theme.s12
            AppToggle {
                id: authToggle
                anchors.verticalCenter: parent.verticalCenter
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                AppText { text: Strings.proxyAuth; role: "bodyStrong" }
                AppText { text: Strings.proxyAuthDesc; role: "small"; color: Theme.muted }
            }
        }

        // Поля логина и пароля (если авторизация включена)
        RowLayout {
            width: parent.width
            spacing: Theme.s12
            visible: authToggle.checked

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: 4
                AppText { text: Strings.proxyUser; role: "caption"; color: Theme.muted }
                AppField {
                    id: userField
                    Layout.fillWidth: true
                    placeholderText: Strings.proxyUser
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: 4
                AppText { text: Strings.proxyPass; role: "caption"; color: Theme.muted }
                AppField {
                    id: passField
                    Layout.fillWidth: true
                    password: true
                    placeholderText: dlg.isEdit ? Strings.proxyPassKeep : Strings.proxyPass
                }
            }
        }

        // Блок проверки соединения и результат
        RowLayout {
            width: parent.width
            spacing: Theme.s12

            SecondaryButton {
                height: 36
                kind: "outline"
                iconName: "refresh"
                text: Strings.proxyTest
                enabled: dlg.testState !== "testing"
                onClicked: dlg.runTest()
            }

            AppText {
                Layout.fillWidth: true
                text: dlg.testText
                role: "small"
                elide: Text.ElideRight
                color: dlg.testState === "ok" ? Theme.ok
                     : dlg.testState === "error" ? Theme.err
                     : Theme.muted
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        // Подвал: кнопки Сохранить / Отмена
        Row {
            anchors.right: parent.right
            spacing: Theme.s12

            SecondaryButton {
                fontRole: "bodyStrong"
                text: Strings.cancel
                onClicked: dlg.close()
            }
            SecondaryButton {
                kind: "accent"
                fontRole: "bodyStrong"
                text: dlg.isEdit ? Strings.proxySave : Strings.proxyAdd
                iconName: "check"
                onClicked: dlg.save()
            }
        }
    }
}
