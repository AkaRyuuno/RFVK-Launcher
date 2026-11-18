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

    // Контекстное меню полей и журнала.
    readonly property string textUndo: "Отменить"
    readonly property string textRedo: "Повторить"
    readonly property string textCut: "Вырезать"
    readonly property string textCopy: "Копировать"
    readonly property string textPaste: "Вставить"
    readonly property string textDelete: "Удалить"
    readonly property string textSelectAll: "Выделить всё"

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

    // ── Вкладки и слоты маршрутизации (прокси) ──
    readonly property string tabAccounts: "Аккаунты"
    readonly property string tabProxies: "Прокси и слоты"
    readonly property string proxiesTitle: "Слоты маршрутизации"
    readonly property string proxiesSubtitle: "До 5 клиентов игры на каждый слот (прямой IP или прокси). Авто-выбор: прямой IP, затем прокси, добавленные вручную, затем проверенные прокси из источников."
    readonly property string addProxy: "Добавить прокси"
    readonly property string editProxy: "Настройки прокси"
    readonly property string proxyDirectDesc: "Локальный IP сетевого адаптера компьютера"
    readonly property string proxyDirectBadge: "Основной слот"
    readonly property string proxyAuthBadge: "Auth"
    function proxyActiveWindows(n) { return "Окон в игре: " + n }
    function proxyAvailableSlots(n) { return "Свободных слотов: " + n }
    readonly property string slotsBusy: "Слоты заняты клиентами"
    readonly property string slotsIdle: "Все слоты свободны"
    readonly property string slotsReady: "Готовы к запуску окон"
    readonly property string slotsAllFull: "Все доступные слоты заполнены"
    readonly property string proxyDllReady: "Сетевой перехват готов"
    readonly property string proxyDllReadyHint: "rfvk_proxy.dll на месте"
    readonly property string proxyDllMissing: "Библиотека не найдена"
    readonly property string proxyDllMissingHint: "Соберите с RFVK_PROXY=ON — без неё запуск через прокси невозможен"
    readonly property string proxyName: "Название"
    readonly property string proxyNameHint: "Например: Германия SOCKS5"
    readonly property string proxyType: "Протокол"
    readonly property string proxyHost: "Хост или IP"
    readonly property string proxyHostHint: "192.168.1.1 или proxy.example.com"
    readonly property string proxyPort: "Порт"
    readonly property string proxyAuth: "Авторизация"
    readonly property string proxyAuthDesc: "Требуется логин и пароль"
    readonly property string proxyUser: "Логин"
    readonly property string proxyPass: "Пароль"
    readonly property string proxyPassKeep: "Пароль (пусто — не менять)"
    readonly property string proxyBadHostPort: "Укажите хост и порт (1–65535)"
    readonly property string proxyTest: "Проверить"
    readonly property string proxyTesting: "Проверка…"
    function proxyTestOk(ms, message) { return message + " · " + ms + " мс" }
    function proxyTestFailed(message) { return "Ошибка: " + message }
    readonly property string proxyPingError: "Ошибка"
    function proxyPingMs(ms) { return ms + " мс" }
    readonly property string proxySave: "Сохранить"
    readonly property string proxyAdd: "Добавить слот"
    readonly property string proxyDelete: "Удалить"
    readonly property string proxyDeleteConfirmTitle: "Удалить прокси?"
    readonly property string proxyDeleteConfirmBody: "Прокси будет удалён из лаунчера. Аккаунты с этим слотом переключатся на авто-выбор."
    readonly property string slotSelectorLabel: "Слот маршрутизации"
    readonly property string slotSelectorHint: "Через какой IP запускать этот аккаунт (до 5 окон на слот)"
    readonly property string slotAuto: "Авто (прямой IP, свободный прокси или источник)"
    readonly property string slotLoad: "Занятость слота"
    readonly property string slotPids: "Клиенты (PID):"
    readonly property string slotNoClients: "Окна игры не запущены"
    readonly property string slotFull: "Лимит исчерпан"
    function slotCapacityText(cur, max) { return cur + "/" + max + " окон" }

    // ── Источники прокси (списки по URL) ──
    readonly property string addSource: "Добавить источник по URL"
    readonly property string editSource: "Настройки источника"
    readonly property string sourcesTitle: "Источники прокси"
    readonly property string sourcesSubtitle: "Списки по ссылке: каждый адрес проверяется через сервер игры, в игру выдаются только рабочие."
    readonly property string sourceUrl: "Ссылка на список"
    readonly property string sourceUrlHint: "https://example.com/proxies/socks5/data.txt"
    readonly property string sourceUrlDesc: "Текстовый файл, один прокси на строку: socks5://host:port, http://user:pass@host:port или host:port"
    readonly property string sourceNameHint: "Например: proxifly SOCKS5"
    readonly property string sourceDefaultType: "Протокол строк без схемы"
    readonly property string sourceAdd: "Добавить и проверить"
    readonly property string sourceBadUrl: "Укажите ссылку http:// или https://"
    readonly property string sourceRefresh: "Скачать список заново"
    readonly property string sourceCheck: "Проверить все адреса"
    readonly property string sourceStop: "Остановить проверку"
    readonly property string sourceFetching: "Загрузка списка…"
    function sourceTotal(n) { return "Адресов: " + n }
    function sourceOk(n) { return "Рабочих: " + n }
    function sourceBad(n) { return "Нерабочих: " + n }
    function sourceUnchecked(n) { return "Не проверено: " + n }
    function sourceInUse(clients, proxies) { return "В игре: " + clients + " окон на " + proxies + " адресах" }
    function sourceProgress(done, total) { return "Проверка " + done + " / " + total }
    function sourceFetchedAt(t) { return "Список от " + t }
    readonly property string sourceDeleteConfirmTitle: "Удалить источник?"
    readonly property string sourceDeleteConfirmBody: "Список и статусы его адресов будут удалены. Добавленные вручную прокси не затрагиваются."
    function slotFromSource(name) { return "Из источника «" + name + "»" }
    readonly property string slotFromSourceHint: "Выдан автоматически · исчезнет, когда закроется последнее окно"
}
