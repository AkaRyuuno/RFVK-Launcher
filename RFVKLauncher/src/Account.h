#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

// Состояние сессии аккаунта. Значения доступны в QML как Controller.Active и т.д.
// (enum объявлен в Controller через Q_ENUM — здесь дублируется только значениями
//  для удобства C++-кода заглушки).
namespace Session {
enum State {
    None = 0,    // нет сохранённой сессии — нужен импорт
    Active,      // сессия действует
    Expiring,    // скоро истечёт — стоит обновить заранее
    Expired      // истекла — нужен импорт
};
}

// Одна запись аккаунта. Передаётся из бэкенда в контроллер целиком.
struct Account {
    QString id;            // стабильный идентификатор (uid или синтетический)
    QString login;         // email / телефон / VK ID (то, что вводил пользователь)
    QString loginKind;     // "email" | "phone" | "vkid" — для подписи в карточке
    QString nick;          // игровой ник
    QString uid;           // VK Play uid
    QString persId;        // RF PersId
    int sessionState = Session::None;
    QDateTime sessionExpiry;   // когда истекает текущая сессия (для Active/Expiring/Expired)
    QDateTime sessionRefreshed; // когда сессию последний раз обновляли
    QString avatarPath;        // путь к аватару (пусто -> рисуем инициалы)
};

Q_DECLARE_METATYPE(Account)
