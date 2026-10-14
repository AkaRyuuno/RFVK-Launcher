# RFVK Launcher

Альтернативный лаунчер RF Online (VK Play) на C++17 + Qt 6 (Qt Quick / QML).
Управляет несколькими игровыми аккаунтами VK Play и запускает игру под
выбранным — или сразу несколько клиентов под несколькими выделенными
аккаунтами.

Авторизация вынесена за интерфейс `IAuthBackend`. Есть две реализации:

- **`VkPlayAuthBackend`** (по умолчанию) — настоящий нативный бэкенд VK Play
  на Qt + Win32, **без Python и без внешних зависимостей**: OAuth
  (`o2-ac/token`), `gem.php` профиль/код запуска, импорт сессии без пароля из
  SSO-cookie встроенного GameCenter (DPAPI + AES-GCM), запуск `RF_Online.exe`.
  Refresh-токены хранятся под DPAPI и в QML/C++ не попадают.
- **`StubAuthBackend`** — заглушка-симулятор (задержки, демо-ошибки, тестовые
  аккаунты) для показа/проверки UI без сети. Включается `RFVK_BACKEND=stub`.

## Стек

- Qt 6.5+ (проверено на **6.11.2 msvc2022_64**), MSVC x64, CMake + Ninja.
- UI — QML (Qt Quick Controls 2, стиль Basic, полностью перерисован).
- Логика и модели — C++ (`QAbstractListModel`, `QObject` с `Q_PROPERTY`/
  сигналами/слотами). Один исполняемый `.exe`.
- Шрифты дизайна встроены в ресурсы: Exo 2 (заголовки), Golos Text (интерфейс),
  IBM Plex Mono (журнал).

## Сборка

Из консоли разработчика x64 (или с вызовом `vcvars64.bat`), добавив Ninja в PATH:

```bat
set PATH=C:\Qt\Tools\Ninja;%PATH%
cmake --preset Debug-x64
cmake --build out\build\debug
out\build\debug\RFVKLauncher.exe
```

Пресеты (`CMakePresets.json`, `CMakeUserPresets.json`) машинно-зависимые и в
репозиторий не входят — их генерирует Qt VS Tools (Qt-версия `6.11.2_msvc2022_64`).
Без пресетов — то же вручную:

```bat
cmake -S . -B out\build\debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
cmake --build out\build\debug
```

Поставка (Release, плоская папка с Qt):

```bat
cmake --preset Release-x64
cmake --build out\build\release
cmake --install out\build\release --prefix out\dist
```

## Структура

```
RFVKLauncher/src    C++: Account, AccountModel (QAbstractListModel),
                    IAuthBackend (интерфейс);
                    VkPlayAuthBackend + VkCrypto + VkChromeCookies (настоящий
                    бэкенд: OAuth/gem.php/импорт cookie, DPAPI, AES-GCM);
                    StubAuthBackend (заглушка-симулятор);
                    Controller (QObject -> QML-синглтон App)
RFVKLauncher/qml    Main + экраны (Sidebar, AccountPanel, EmptyState,
                    GameDirRow, LogConsole, диалоги, тосты);
                    Theme/Strings — синглтоны (токены дизайна и тексты);
                    qml/components — AppText, кнопки, поле, бейдж, спиннер
RFVKLauncher/resources/fonts  встроенные шрифты (OFL)
injector/           rfvk_inject.cpp — 32-битный LoadLibrary-инжектор HUD
                    (отдельная цель, рядом с лаунчером); build-injector.bat
```

## Архитектура бэкенда

`Controller` зависит только от `IAuthBackend` (операции `importSession`,
`launch`, `refreshSession`, `deleteAccount` + сигналы
`progress`/`finished`/`accountsChanged`/`busyChanged`/`error`/`readyToLaunch`).
Выбор реализации — в `main.cpp` по `RFVK_BACKEND` (по умолчанию настоящий).

Действия над несколькими выделенными аккаунтами `Controller` ставит в очередь
и подаёт бэкенду по одной операции (следующая — после `busyChanged(false)`
предыдущей). Ошибки запоминаются по каждому аккаунту, в конце пакета —
сводка «Готово: N из M».

`VkPlayAuthBackend` ведёт сеть в рабочем потоке (`QThread` + `QNetworkAccessManager`),
результаты отдаёт сигналами. Запуск игры делает `Controller` по `readyToLaunch`
(у него каталог игры), стартуя `RF_Online.exe --sz_token=<code> --sz_pers_id=<persId>
-my_ui_locale_arg=ru-RU -my_ui_currency_arg=RUB`.

Аккаунт добавляется только импортом сессии из GameCenter (вход по паролю
убран): войдите в GameCenter под нужным аккаунтом и нажмите «Импортировать»;
повторите для каждого аккаунта.

## Хранение

- Настройки (каталог игры, тема, выделенные аккаунты) — `QSettings` (INI,
  `%APPDATA%\RFVK\RFVKLauncher.ini`).
- Аккаунты настоящего бэкенда — `AppDataLocation\accounts_vk.json`: refresh-токен
  хранится **зашифрованным DPAPI** (привязка к пользователю Windows),
  в QML/C++-слой не передаётся.
- Аккаунты заглушки — отдельный `AppDataLocation\accounts.json` (демо-данные).

## Улучшенный HUD (опционально)

Настройка «Улучшенный HUD в игре» (в диалоге настроек) после запуска клиента
автоматически подключает к нему библиотеку улучшенного интерфейса `seirin.dll`.
Библиотека **не входит в поставку** — пользователь кладёт её рядом с
`RFVKLauncher.exe` сам; нет файла — шаг подключения молча пропускается.

Клиент `RF_Online.exe` 32-битный, а лаунчер 64-битный, поэтому инжект делает
отдельный 32-битный `rfvk_inject.exe` (той же разрядности, что цель) —
классический `LoadLibrary`-инжект, как `tools/inject.ps1` в проекте seirin, но
без PowerShell. Лаунчер (`src/HudInjector.*`) дожидается появления окна клиента
и зовёт инжектор через `QProcess`; неудача не срывает запуск игры. Инжектор
собирается отдельной 32-битной целью CMake (`injector/`, через `vcvarsall x86`,
путь — в кэш-переменной `RFVK_VCVARSALL`); нет x86-тулчейна — собирается только
лаунчер, автоподключение HUD неактивно.

## Темы и управление

- Светлая/тёмная тема, переключатель в верхней полосе и по `Ctrl+T`; выбор
  запоминается. Остальные настройки — в диалоге по кнопке настроек в верхней полосе.
- Двойной клик по карточке аккаунта — запуск под ним.
- Несколько аккаунтов: `Ctrl`+клик — добавить/убрать, `Shift`+клик — диапазон,
  `Ctrl+A` — все. Справа тогда сводная панель; «Запустить RF Online · N»,
  «Обновить сессию» и корзина применяются ко всем выделенным.
- Иконка приложения — эмблема-октагон из верхней полосы
  (`resources/app.ico` в exe через `app.rc`, `resources/app.png` — иконка окна).
