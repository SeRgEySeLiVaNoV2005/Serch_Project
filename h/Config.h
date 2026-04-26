#pragma once
#include <string>

struct Config {
    // Параметры БД
    std::string db_host;
    int db_port;
    std::string db_name;
    std::string db_user;
    std::string db_password;

    // Параметры Паука
    std::string start_url;
    int recursion_depth;

    // Параметры сервера
    int server_port;

    // Метод для загрузки из файла
    static Config load(const std::string& filename);
};