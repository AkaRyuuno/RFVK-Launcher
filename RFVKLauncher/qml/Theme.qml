pragma Singleton
import QtQuick
import Rfvk

// Токены дизайна RF Launcher: палитра (тёмная/светлая), типографика, радиусы,
// отступы. Переключение темы — через App.theme ("dark"/"light").
QtObject {
    id: theme

    readonly property bool dark: App.theme !== "light"

    // ── Палитра. Каждая роль — пара {тёмная, светлая}. ──
    function pick(d, l) { return dark ? d : l }

    readonly property color bg:           pick("#0D1015", "#EEF0F3") // фон окна, основная область
    readonly property color surface:      pick("#141920", "#FFFFFF") // шапка, сайдбар, статус, диалоги
    readonly property color raised:       pick("#1B222B", "#F5F6F8") // поля ввода, тост, вторичные кнопки
    readonly property color border:       pick("#29323E", "#D7DBE1") // разделители, обводки 1px
    readonly property color borderStrong: pick("#3A4552", "#BCC3CC") // обводка тоста, hover полей
    readonly property color text:         pick("#E7EBF0", "#13171D") // основной текст
    readonly property color muted:        pick("#8C98A8", "#566170") // подписи, логины, время в логе

    readonly property color accent:       pick("#F2A33A", "#F0A13A") // главная кнопка, выделение, фокус
    readonly property color accentHover:  pick("#F7B65C", "#F5B256")
    readonly property color accentPress:  pick("#D98C24", "#DB8B22")
    readonly property color accentInk:    "#1A1204"                  // текст на акценте (контраст 9:1)
    readonly property color accentText:   pick("#F2A33A", "#8F5300") // акцентный текст на фоне

    readonly property color ok:           pick("#3DBE7E", "#1E7F4F") // сессия активна, успех
    readonly property color warn:         pick("#E9C24B", "#8A6400") // скоро истечёт
    readonly property color err:          pick("#EF6A57", "#C23A28") // истекла, ошибки, баннер
    readonly property color danger:       "#C9392B"                  // фон кнопки «Удалить» (текст белый)
    readonly property color dangerText:   "#FFFFFF"
    readonly property color consoleBg:    pick("#0A0D11", "#F7F8FA") // фон консоли лога

    // Заливка бейджа статуса — цвет статуса с прозрачностью ~13%.
    function badgeFill(c) { return Qt.rgba(c.r, c.g, c.b, 0.13) }

    // Цвет статуса сессии по состоянию (Controller.SessionState).
    function statusColor(state) {
        switch (state) {
        case App.Active: return ok
        case App.Expiring: return warn
        case App.Expired: return err
        default: return muted
        }
    }

    // ── Типографика. ──
    readonly property string fontH: "Exo 2"        // заголовки, главная кнопка
    readonly property string fontUI: "Golos Text"  // интерфейс, текст
    readonly property string fontMono: "IBM Plex Mono" // консоль лога

    // ── Радиусы. ──
    readonly property int rBadge: 6
    readonly property int rField: 8
    readonly property int rCard: 10
    readonly property int rMain: 12
    readonly property int rDialog: 14
    readonly property int rPill: 999

    // ── Отступы. ──
    readonly property int s4: 4
    readonly property int s8: 8
    readonly property int s12: 12
    readonly property int s16: 16
    readonly property int s20: 20
    readonly property int s24: 24
    readonly property int s32: 32
    readonly property int s40: 40

    // ── Размеры. ──
    readonly property int hField: 40   // поля/вторичные кнопки
    readonly property int hMain: 56    // главная кнопка
    readonly property int sidebarW: 304

    // ── Аватары: цвет по хэшу идентификатора аккаунта. ──
    readonly property var avatarPalette: ["#2FB4A6", "#8B7BE8", "#4CAF7D", "#4A9EEA",
                                          "#E8954A", "#E85A8B", "#6CA0DC", "#C06BD8"]
    function avatarColor(seed) {
        var h = 0
        var s = "" + seed
        for (var i = 0; i < s.length; i++)
            h = (h * 31 + s.charCodeAt(i)) >>> 0
        return avatarPalette[h % avatarPalette.length]
    }
    function avatarFill(c) { return Qt.rgba(c.r, c.g, c.b, 0.22) }
}
