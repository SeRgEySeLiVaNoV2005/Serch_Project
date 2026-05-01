#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#endif


#include <iostream>
#include <string>
#include <thread> 
#include "Config.h"
#include "Database.h"
#include "Spider.h"
#include "SearchServer.h"

int main() {
    try {
        Config cfg = Config::load("config.ini");

        std::string conn_str = "host=" + cfg.db_host + " port=" + std::to_string(cfg.db_port) +
            " dbname=" + cfg.db_name + " user=" + cfg.db_user +
            " password=" + cfg.db_password;

        Database db(conn_str);
        db.create_tables();

        std::thread spider_thread([&db, cfg]() {
            try {
                std::cout << "[Background] Spider started indexing: " << cfg.start_url << std::endl;
                Spider spider(db, 32); 
                spider.run(cfg.start_url, cfg.recursion_depth);
                std::cout << "[Background] Spider has finished indexing!" << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "[Background Error] Spider failed: " << e.what() << std::endl;
            }
            });

        spider_thread.detach();

        std::cout << "[Main] Search Server starting on http://localhost:" << cfg.server_port << std::endl;

        SearchServer server(db, "0.0.0.0", static_cast<unsigned short>(cfg.server_port));
        server.run(); 

    }
    catch (const std::exception& e) {
        std::cerr << "Critical error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}