#include "AccountModel.h"
#include "Controller.h"
#include "StubAuthBackend.h"
#include "VkPlayAuthBackend.h"

#include <QDirIterator>
#include <QFontDatabase>
#include <QIcon>
#include <QImage>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml>

// Загрузка встроенных шрифтов дизайна: Exo 2 (заголовки, главная кнопка),
// Golos Text (интерфейс), IBM Plex Mono (консоль лога).
static void loadFonts()
{
    QDirIterator it(QStringLiteral(":/qt/qml/Rfvk/resources/fonts"), {QStringLiteral("*.ttf")},
                    QDir::Files);
    while (it.hasNext())
        QFontDatabase::addApplicationFont(it.next());
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("RFVK"));
    QGuiApplication::setApplicationName(QStringLiteral("RFVKLauncher"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("RFVK Launcher"));
    QGuiApplication::setApplicationVersion(QStringLiteral(RFVK_VERSION));
    // Иконка окна и панели задач — эмблема-октагон из верхней полосы.
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Rfvk/resources/app.png")));

    qRegisterMetaType<Account>("Account");
    qRegisterMetaType<QVector<Account>>("QVector<Account>");

    loadFonts();
    QQuickStyle::setStyle(QStringLiteral("Basic")); // контролы полностью перерисованы в QML

    // Сборка слоёв: модель <- контроллер <- бэкенд.
    // По умолчанию — настоящий бэкенд VK Play (нативный, без Python).
    // RFVK_BACKEND=stub включает заглушку-симулятор (для показа дизайна).
    auto *model = new AccountModel(&app);
    IAuthBackend *backend = nullptr;
    if (qEnvironmentVariable("RFVK_BACKEND") == QLatin1String("stub"))
        backend = new StubAuthBackend(&app);
    else
        backend = new VkPlayAuthBackend(&app);
    auto *controller = new Controller(model, backend, &app);

    // Переопределения для скриншотов/отладки: RFVK_THEME=dark|light,
    // RFVK_SELECT=<индекс>[,<индекс>…] (несколько — мультивыбор).
    if (qEnvironmentVariableIsSet("RFVK_THEME"))
        controller->setTheme(qEnvironmentVariable("RFVK_THEME"));
    if (qEnvironmentVariableIsSet("RFVK_SELECT")) {
        bool first = true;
        for (const QString &s : qEnvironmentVariable("RFVK_SELECT").split(QLatin1Char(','))) {
            const int i = s.trimmed().toInt();
            if (i < 0 || i >= model->rowCount())
                continue;
            if (first)
                controller->selectById(model->at(i).id);
            else
                controller->toggleSelect(model->at(i).id);
            first = false;
        }
    }
    if (qEnvironmentVariableIsSet("RFVK_GAMEDIR"))
        controller->setGameDir(qEnvironmentVariable("RFVK_GAMEDIR"));

    // Контроллер доступен в QML как синглтон App (import Rfvk):
    // App.importSession(), App.selectedAccount, App.Active и т.д.
    qmlRegisterSingletonInstance("Rfvk", 1, 0, "App", controller);

    QQmlApplicationEngine engine;
    // Для скриншотов: RFVK_OPEN=add открывает диалог добавления при старте.
    engine.rootContext()->setContextProperty("StartupOpen", qEnvironmentVariable("RFVK_OPEN"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Rfvk", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
    controller->logStartup();

    // Режим скриншота (для визуальной сверки с макетом): RFVK_SHOT=путь.png —
    // через 1.8 c окно снимается в PNG и приложение закрывается.
    if (qEnvironmentVariableIsSet("RFVK_SHOT")) {
        const QString shot = qEnvironmentVariable("RFVK_SHOT");
        const int ms = qEnvironmentVariableIsSet("RFVK_SHOT_MS") ? qEnvironmentVariable("RFVK_SHOT_MS").toInt() : 1800;
        if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            QTimer::singleShot(ms, w, [w, shot]() {
                const QImage img = w->grabWindow();
                img.save(shot);
                QCoreApplication::quit();
            });
        }
    }

    return QGuiApplication::exec();
}
