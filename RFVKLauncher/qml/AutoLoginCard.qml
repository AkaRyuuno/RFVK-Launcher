import QtQuick
import Rfvk

// Карточка «Автовход» в панели аккаунта: FG-пароль и персонаж, которым входить.
// Сам пароль в QML не приходит — только признак «задан» (acc.fgSet).
Rectangle {
    id: card
    property var acc
    readonly property string accId: acc && acc.id ? acc.id : ""
    readonly property var chars: acc && acc.characters ? acc.characters : []
    readonly property bool active: App.autoLoginEnabled && App.autoLoginAvailable

    radius: Theme.rCard
    color: Theme.surface
    border.width: 1
    border.color: Theme.border
    implicitHeight: col.implicitHeight + 2 * Theme.s12

    // Другой аккаунт — поле пароля пустое, выбор персонажа — его.
    onAccIdChanged: fgField.text = ""
    onAccChanged: syncChar()
    Component.onCompleted: syncChar()

    function charModel() {
        var m = [{ label: Strings.autoCharNone, value: "" }]
        for (var i = 0; i < chars.length; i++) {
            var c = chars[i]
            m.push({ label: Strings.charLabel(c.name, c.level, c.race), value: c.name })
        }
        return m
    }
    function syncChar() {
        var want = acc && acc.autoChar ? acc.autoChar : ""
        var idx = 0
        for (var i = 0; i < chars.length; i++)
            if (chars[i].name === want) idx = i + 1
        charSelect.currentIndex = idx
    }
    function saveFg() {
        if (!fgField.text.length) return
        App.setAccountFg(card.accId, fgField.text)
        fgField.text = ""
    }

    Column {
        id: col
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.s12
        anchors.leftMargin: Theme.s16
        anchors.rightMargin: Theme.s16
        spacing: Theme.s8

        // Заголовок: название + шаги (или почему автовход не работает).
        Row {
            width: parent.width
            spacing: Theme.s8
            Icon { anchors.verticalCenter: parent.verticalCenter; name: "key"; size: 16; color: card.active ? Theme.accentText : Theme.muted }
            AppText { id: ttl; anchors.verticalCenter: parent.verticalCenter; text: Strings.autoTitle; role: "bodyStrong" }
            AppText {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 16 - ttl.width - 2 * Theme.s8
                elide: Text.ElideRight
                role: "small"
                color: App.autoLoginEnabled && !App.autoLoginAvailable ? Theme.warn : Theme.muted
                text: !App.autoLoginEnabled ? Strings.autoOff
                    : !App.autoLoginAvailable ? Strings.autoMissing
                    : Strings.autoSteps
            }
        }

        Row {
            width: parent.width
            spacing: Theme.s16
            opacity: card.active ? 1 : 0.55

            // ── FG-пароль ──
            Row {
                width: (parent.width - Theme.s16) / 2
                spacing: Theme.s8
                AppField {
                    id: fgField
                    width: parent.width - saveBtn.width - (clearBtn.visible ? clearBtn.width + Theme.s8 : 0) - Theme.s8
                    password: true
                    maximumLength: 12
                    validator: RegularExpressionValidator { regularExpression: /[\x21-\x7e]*/ }
                    placeholderText: card.acc && card.acc.fgSet ? Strings.autoFgReplace : Strings.autoFgEmpty
                    enabled: card.accId.length > 0
                    onAccepted: card.saveFg()
                }
                SecondaryButton {
                    id: saveBtn
                    kind: "outline"; text: Strings.autoFgSave; fontRole: "small"
                    enabled: fgField.text.length > 0
                    onClicked: card.saveFg()
                }
                // Удалить сохранённый пароль — дальше вводить в игре вручную.
                Rectangle {
                    id: clearBtn
                    visible: card.acc && card.acc.fgSet
                    width: Theme.hField; height: Theme.hField; radius: Theme.rField
                    color: clrHover.hovered ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.12) : "transparent"
                    border.width: 1; border.color: clrHover.hovered ? Theme.danger : Theme.border
                    Icon { anchors.centerIn: parent; name: "trash"; size: 16; color: clrHover.hovered ? Theme.danger : Theme.muted }
                    HoverHandler { id: clrHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: App.setAccountFg(card.accId, "") }
                }
            }

            // ── Персонаж ──
            AppSelect {
                id: charSelect
                width: (parent.width - Theme.s16) / 2
                model: card.charModel()
                enabled: card.chars.length > 0
                displayText: card.chars.length > 0 ? currentText : Strings.autoCharUnknown
                onModelChanged: card.syncChar()
                onActivated: App.setAccountCharacter(card.accId, currentValue)
            }
        }
    }
}
