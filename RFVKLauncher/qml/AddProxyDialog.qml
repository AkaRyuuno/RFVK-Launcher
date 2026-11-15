import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Rfvk

// Диалог добавления и редактирования SOCKS5 / HTTP прокси-серверов.
Popup {
    id: dlg
    property bool isEdit: false
    property string editSlotId: ""
    property string testResultText: ""
    property int testResultPing: -1
    property bool testing: false

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

    // Слушатель результатов проверки соединения
    Connections {
        target: ProxyManager
        function onCustomTestFinished(ok, pingMs, message) {
            dlg.testing = false
            dlg.testResultPing = pingMs
            if (ok) {
                dlg.testResultText = "Соединение успешно (" + pingMs + " мс)"
            } else {
                dlg.testResultText = message.length > 0 ? ("Ошибка: " + message) : "Не удалось подключиться к прокси"
            }
        }
    }

    function openAdd(withAuth) {
        dlg.isEdit = false
        dlg.editSlotId = ""
        nameField.text = "Proxy " + (ProxyManager.model.rowCount() || 1)
        typeSelect.currentIndex = 0 // SOCKS5
        hostField.text = ""
        portField.text = "1080"
        authToggle.checked = !!withAuth
        userField.text = ""
        passField.text = ""
        dlg.testResultText = ""
        dlg.testResultPing = -1
        dlg.testing = false
        dlg.open()
    }

    function openEdit(slot) {
        dlg.isEdit = true
        dlg.editSlotId = slot.slotId
        nameField.text = slot.name
        typeSelect.currentIndex = (slot.type === 2 ? 1 : 0)
        hostField.text = slot.host
        portField.text = slot.port.toString()
        authToggle.checked = slot.authEnabled
        userField.text = slot.username
        passField.text = slot.password
        dlg.testResultText = ""
        dlg.testResultPing = -1
        dlg.testing = false
        dlg.open()
    }

    function runTest() {
        var host = hostField.text.trim()
        var port = parseInt(portField.text.trim(), 10)
        if (!host.length || isNaN(port) || port <= 0 || port > 65535) {
            dlg.testResultPing = -2
            dlg.testResultText = "Укажите корректный хост и порт (1–65535)"
            return
        }
        dlg.testing = true
        dlg.testResultText = Strings.proxyTesting
        dlg.testResultPing = -1
        ProxyManager.testCustomProxy(
            typeSelect.currentValue,
            host,
            port,
            authToggle.checked,
            userField.text.trim(),
            passField.text
        )
    }

    function save() {
        var name = nameField.text.trim()
        var host = hostField.text.trim()
        var port = parseInt(portField.text.trim(), 10)
        if (!name.length) name = "Proxy " + host
        if (!host.length) {
            dlg.testResultPing = -2
            dlg.testResultText = "Хост не может быть пустым"
            return
        }
        if (isNaN(port) || port <= 0 || port > 65535) {
            dlg.testResultPing = -2
            dlg.testResultText = "Некорректный порт (1–65535)"
            return
        }

        if (dlg.isEdit) {
            ProxyManager.updateProxy(
                dlg.editSlotId,
                name,
                typeSelect.currentValue,
                host,
                port,
                authToggle.checked,
                userField.text.trim(),
                passField.text,
                true
            )
        } else {
            ProxyManager.addProxy(
                name,
                typeSelect.currentValue,
                host,
                port,
                authToggle.checked,
                userField.text.trim(),
                passField.text
            )
        }
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
                placeholderText: "Например: Германия SOCKS5"
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
                placeholderText: "192.168.1.1 или domain.com"
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
                    placeholderText: "Логин"
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
                    placeholderText: "Пароль"
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
                enabled: !dlg.testing
                onClicked: dlg.runTest()
            }

            AppText {
                Layout.fillWidth: true
                text: dlg.testResultText
                role: "small"
                elide: Text.ElideRight
                color: dlg.testResultPing >= 0 ? Theme.ok
                     : dlg.testResultPing === -2 ? Theme.err
                     : Theme.muted
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        // Подвал: кнопки Сохранить / Отмена
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
                    text: dlg.isEdit ? Strings.proxySave : Strings.proxyAdd
                    iconName: "check"
                    onClicked: dlg.save()
                }
            }
        }
    }
}
