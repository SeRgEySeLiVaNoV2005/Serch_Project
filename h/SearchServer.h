#pragma once
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "Database.h"

namespace http = boost::beast::http;

class SearchServer {
public:
    SearchServer(Database& db, const std::string& address, unsigned short port);
    void run();

private:
    std::string url_decode(std::string str);
    std::vector<std::string> parse_query(std::string body);
    void handle_request(http::request<http::string_body>&& req,
        http::response<http::string_body>& res);
    std::string read_file(const std::string& path);
    Database& db;
    std::string address;
    unsigned short port;
};