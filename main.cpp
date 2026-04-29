#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#endif


#include <iostream>
#include <locale.h>
#include <string>
#include "Config.h"
#include "Database.h"
#include "Spider.h"
#include "SearchServer.h"

int main() {
#ifdef _WIN32
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
#endif
    setlocale(LC_ALL, "Rus");
    try {
        Config cfg = Config::load("config.ini");

        std::string conn_str =
            "host=" + cfg.db_host +
            " port=" + std::to_string(cfg.db_port) +
            " dbname=" + cfg.db_name +
            " user=" + cfg.db_user +
            " password=" + cfg.db_password;

        Database db(conn_str);
        db.create_tables();

        std::cout << "=== Запуск индексации (Паук) ===" << std::endl;
        std::cout << "Стартовый URL: " << cfg.start_url << std::endl;

        Spider spider(db, 32);
        spider.run(cfg.start_url, cfg.recursion_depth);

        std::cout << "=== Индексация завершена! ===" << std::endl;

        std::cout << "=== Запуск поискового сервера ===" << std::endl;
        std::cout << "Открыть в браузере: http://localhost:" << cfg.server_port << std::endl;

        SearchServer server(db, "0.0.0.0", static_cast<unsigned short>(cfg.server_port));
        server.run(); 

    }
    catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}