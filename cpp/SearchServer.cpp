#include "SearchServer.h"
#include <boost/beast/core.hpp>
#include <boost/locale.hpp>
#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <filesystem>

namespace fs = std::filesystem;
namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

std::string get_mime_type(const std::string& path_str) {
    fs::path p(path_str);
    std::string ext = p.extension().string(); 

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";

    return "text/plain; charset=utf-8";
}

std::string read_file_content(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

SearchServer::SearchServer(Database& db, const std::string& address, unsigned short port)
    : db(db), address(address), port(port) {
}

void SearchServer::run() {
    boost::asio::io_context ioc;
    tcp::acceptor acceptor{ ioc, {boost::asio::ip::make_address(address), port} };

    std::cout << "Сервер запущен на http://" << address << ":" << port << std::endl;

    for (;;) {
        tcp::socket socket{ ioc };
        acceptor.accept(socket);

        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        http::response<http::string_body> res;
        handle_request(std::move(req), res);

        res.set(http::field::content_type, "text/html; charset=utf-8");
        res.set(http::field::server, "Beast");
        res.prepare_payload();
        http::write(socket, res);
    }
}
std::string SearchServer::url_decode(std::string str) {
    std::string res;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '+') res += ' ';
        else if (str[i] == '%' && i + 2 < str.length()) {
            std::string hex = str.substr(i + 1, 2);
            char c = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            res += c;
            i += 2;
        }
        else res += str[i];
    }
    return res;
}

std::vector<std::string> SearchServer::parse_query(std::string body) {
    if (body.find("query=") == 0) body.erase(0, 6);

    std::string decoded_body = url_decode(body);

    boost::locale::generator gen;
    std::locale loc = gen("en_US.UTF-8");
    decoded_body = boost::locale::to_lower(decoded_body, loc);

    std::vector<std::string> words;
    std::stringstream ss(decoded_body);
    std::string word;
    while (ss >> word) {
        if (word.length() >= 3) words.push_back(word);
    }
    return words;
}

void SearchServer::handle_request(http::request<http::string_body>&& req,
    http::response<http::string_body>& res) {

    std::string target = std::string(req.target());
    target = url_decode(target);

    if (req.method() == http::verb::get) {
        std::string relative_path = (target == "/") ? "/index.html" : target;
        std::string full_path = "html" + relative_path;

        if (fs::exists(full_path) && !fs::is_directory(full_path)) {
            res.body() = read_file_content(full_path);
            res.result(http::status::ok);
            res.set(http::field::content_type, get_mime_type(full_path));
        }
        else {
            res.result(http::status::not_found);
            res.body() = "404 Not Found";
            res.set(http::field::content_type, "text/plain");
        }
    }
    else if (req.method() == http::verb::post) {
        auto words = parse_query(req.body());
        auto results = db.search(words);

        std::string html_fragment;

        if (results.empty()) {
            html_fragment = "<p style='color: rgba(255,255,255,0.5); font-size: 1.2rem; margin-top: 50px;'>No results found.</p>";
        }
        else {
            for (const auto& item : results) {
                html_fragment += "<div class='result-card'>";
                html_fragment += "<cite>" + item.url + "</cite>";
                html_fragment += "<a href='" + item.url + "' target='_blank'>" + item.url + "</a>";
                html_fragment += "<span>Relevance Score: " + std::to_string(item.score) + "</span>";
                html_fragment += "</div>";
            }
        }

        res.body() = html_fragment;
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/html; charset=utf-8");
    }

    res.prepare_payload();
}

std::string SearchServer::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "Ошибка: файл " + path + " не найден!";
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}