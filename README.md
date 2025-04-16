Серверное приложение для аутентификации и регистрации пользователей через TCP-соединения с использованием PostgreSQL и JSON.

Перед запуском сервера необходимо:
Создать базу данных PostgreSQL и пользователя (параметры по умолчанию указаны в DB_CONNECTION_STRING)
Создать таблицы users и user_documents в БД

## 📋 Функционал

-  Аутентификация пользователей по логину/паролю
-  Регистрация новых пользователей
-  Хранение полной пользовательской информации
-  Работа с документами пользователей
-  Безопасное хранение паролей (Base64)

## 🛠 Технологии

- C++17
- Boost.Asio (асинхронный сетевой ввод-вывод)
- Boost.PropertyTree (работа с JSON)
- libpqxx (доступ к PostgreSQL)
