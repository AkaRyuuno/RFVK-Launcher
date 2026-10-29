pragma Singleton
import QtQuick
import Rfvk

// Тексты интерфейса (русский). Вынесены отдельно, чтобы не разбрасывать строки
// по вьюхам и при желании добавить второй язык.
QtObject {
    readonly property string appTitle: "RF Launcher"
    readonly property string brandPill: "RF Online · VK Play"
    readonly property string accounts: "АККАУНТЫ"

    // Главная панель.
    readonly property string launch: "Запустить RF Online"
    readonly property string launchRetry: "Повторить запуск"
    readonly property string refresh: "Обновить сессию"
    readonly property string remove: "Удалить аккаунт"
    readonly property string addAccount: "Добавить аккаунт"
    readonly property string quickImport: "Импорт из GameCenter"
    readonly property string errorBannerTitle: "Не удалось выполнить операцию"

    // Несколько выделенных аккаунтов.
    readonly property string multiHint: "Действия — для всех выделенных"
    readonly property string sidebarMultiHint: "Ctrl / Shift + клик — выбрать несколько"
    function versionLabel(v) { return "RFVK Launcher " + v }
    readonly property string removeFromSelection: "Убрать из выделения"
    function selectedTitle(n) { return "Выбрано: " + accountsN(n) }
    function launchN(n) { return "Запустить RF Online · " + n }
    function noSessionWarn(n) { return "У " + accountsGen(n) + " нет действующей сессии — нужен импорт из GameCenter" }

    // «1 аккаунт», «2 аккаунта», «5 аккаунтов».
    function accountsN(n) {
        var m10 = n % 10, m100 = n % 100
        if (m10 === 1 && m100 !== 11) return n + " аккаунт"
        if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) return n + " аккаунта"
        return n + " аккаунтов"
    }
    // Родительный падеж: «у 1 аккаунта», «у 5 аккаунтов».
    function accountsGen(n) {
        var m10 = n % 10, m100 = n % 100
        return n + (m10 === 1 && m100 !== 11 ? " аккаунта" : " аккаунтов")
    }

    // Добавление аккаунта.
    readonly property string importTitle: "Импорт сессии из GameCenter"
    readonly property string importButton: "Импортировать из GameCenter"
    readonly property string importHint: "Возьмём текущую сессию из установленного VK Play GameCenter — пароль не нужен. Чтобы добавить другой аккаунт, войдите под ним в GameCenter и повторите импорт."
    readonly property string loginPrefix: "Вход:"

    // Каталог игры.
    readonly property string gameDir: "КАТАЛОГ ИГРЫ"
    readonly property string gameDirEmpty: "Путь к папке RF Online не задан"
    readonly property string browse: "Обзор…"
    readonly property string rfFound: "RF_Online.exe найден"
    readonly property string rfNotFound: "RF_Online.exe не найден"
    readonly property string gcFound: "GameCenter: найден"
    readonly property string gcNotFound: "GameCenter: не найден"

    // Консоль лога.
    readonly property string log: "ЖУРНАЛ"
    readonly property string logEmpty: "Здесь появятся шаги запуска: обновление сессии, код запуска, старт игры."
    readonly property string clear: "Очистить"

    // Нижняя строка состояния.
    readonly property string ready: "Готово"
    readonly property string working: "Выполняется:"
    readonly property string noAccounts: "Нет аккаунтов"
    readonly property string errorPrefix: "Ошибка:"

    // Пустое состояние (правая панель-онбординг).
    readonly property string emptyBigTitle: "Добавьте первый аккаунт"
    readonly property string emptyBigSubtitle: "Лаунчер хранит сессии ваших аккаунтов VK Play и запускает RF Online под любым из них в один клик."
    readonly property string mainMethod: "ОСНОВНОЙ СПОСОБ"
    readonly property string methodImportTitle: "Импорт сессии"
    readonly property string methodImportDesc: "Без пароля — из установленного VK Play GameCenter. Войдите в GameCenter под нужным аккаунтом и нажмите «Импортировать». Повторите для каждого аккаунта."
    readonly property string importNow: "Импортировать из GameCenter"

    // Пустой сайдбар.
    readonly property string sidebarEmptyTitle: "Аккаунтов пока нет"
    readonly property string sidebarEmptyHint: "Добавьте аккаунт, чтобы запускать игру в один клик"

    // GameCenter не найден (баннер в диалоге).
    readonly property string gcWarnTitle: "GameCenter не найден."
    readonly property string gcWarnBody: "Установите VK Play GameCenter и войдите в аккаунт, затем повторите — или укажите путь вручную."
    readonly property string specifyPath: "Указать путь…"

    // Пустое состояние.
    readonly property string emptyTitle: "Нет аккаунтов"
    readonly property string emptyHint: "Добавьте аккаунт: импортируйте сессию из GameCenter."

    // Настройки.
    readonly property string settingsTitle: "Настройки"
    readonly property string settingsThemeTitle: "Тёмная тема"
    readonly property string settingsThemeDesc: "Тёмное оформление лаунчера. Выключите — будет светлое."
    readonly property string settingsHudTitle: "Улучшенный HUD в игре"
    readonly property string settingsHudDesc: "После запуска клиента автоматически подключать библиотеку улучшенного интерфейса (seirin.dll), если она лежит рядом с лаунчером. Не нравится вид — выключите."
    readonly property string settingsHudMissingTitle: "Библиотека не найдена."
    readonly property string settingsHudMissingBody: "Положите seirin.dll в папку с лаунчером. Пока её нет, подключение при запуске пропускается."

    // Автовход (карточка в панели аккаунта и настройка).
    readonly property string autoTitle: "Автовход"
    readonly property string autoSteps: "Сервер 1 → «Начать игру» → FG → персонаж"
    readonly property string autoOff: "Выключен в настройках"
    readonly property string autoMissing: "Нет rfvk_autologin.dll рядом с лаунчером"
    readonly property string autoFgEmpty: "FG не задан"
    readonly property string autoFgReplace: "FG сохранён"
    readonly property string autoFgSave: "Сохранить"
    readonly property string autoCharNone: "Не выбирать — выберу сам"
    readonly property string autoCharUnknown: "Персонажи — после входа"
    function charLabel(name, level, race) {
        var parts = [name]
        if (level > 0) parts.push(level + " ур.")
        if (race && race.length) parts.push(race)
        return parts.join(" · ")
    }
    readonly property string settingsAutoTitle: "Автовход в игру"
    readonly property string settingsAutoDesc: "После запуска клиент сам выбирает «Сервер 1», нажимает «Начать игру», вводит FG-пароль и входит выбранным персонажем. Пароль и персонаж задаются в панели аккаунта."
    readonly property string settingsAutoMissingTitle: "Не найден компонент лаунчера."
    readonly property string settingsAutoMissingBody: "Рядом с лаунчером нет rfvk_autologin.dll — автовход не сработает. Переустановите лаунчер."

    // Диалог удаления.
    readonly property string confirmDeleteTitle: "Удалить аккаунт?"
    readonly property string confirmDeleteYes: "Удалить"
    readonly property string cancel: "Отмена"

    // Заголовок статуса сессии (крупный, в панели).
    function statusTitle(state) {
        switch (state) {
        case App.Active: return "Сессия активна"
        case App.Expiring: return "Сессия скоро истечёт"
        case App.Expired: return "Сессия истекла"
        default: return "Нет сохранённой сессии"
        }
    }

    // Короткая подпись-бейдж.
    function statusBadge(state) {
        switch (state) {
        case App.Active: return "Активна"
        case App.Expiring: return "Истекает"
        case App.Expired: return "Истекла"
        default: return "Нет сессии"
        }
    }

    // Подпись способа входа.
    function loginKindLabel(kind) {
        switch (kind) {
        case "email": return "Email"
        case "phone": return "Телефон"
        case "vkid": return "VK ID"
        default: return kind
        }
    }
}
