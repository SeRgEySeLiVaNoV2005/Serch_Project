#include "Config.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

Config Config::load(const std::string& filename) {
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(filename, pt);

    Config cfg;
    cfg.db_host = pt.get<std::string>("database.host", "localhost");
    cfg.db_port = pt.get<int>("database.port", 5432);
    cfg.db_name = pt.get<std::string>("database.dbname");
    cfg.db_user = pt.get<std::string>("database.user");
    cfg.db_password = pt.get<std::string>("database.password");

    cfg.start_url = pt.get<std::string>("spider.start_url");
    cfg.recursion_depth = pt.get<int>("spider.recursion_depth", 1);

    cfg.server_port = pt.get<int>("server.port", 8080);

    return cfg;
}