#include "Spider.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <iomanip>
#include <openssl/ssl.h>
#include <regex>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

void parse_url_thread(std::string url, std::string& host, std::string& target) {
    if (url.find("http://") == 0) url.erase(0, 7);
    auto pos = url.find('/');
    if (pos == std::string::npos) {
        host = url; target = "/";
    }
    else {
        host = url.substr(0, pos); target = url.substr(pos);
    }
}

Spider::Spider(Database& database, int num_threads)
    : db(database), pool(num_threads) {
}

void Spider::run(const std::string& start_url, int max_depth) {
    active_tasks = 1;

    net::post(pool, [this, start_url, max_depth]() {
        process_url(start_url, 1, max_depth);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (active_tasks > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    pool.stop(); 
    pool.join();
}

void Spider::process_url(std::string url, int current_depth, int max_depth) {
    {
        std::lock_guard<std::mutex> lock(visited_mtx);
        if (visited_urls.count(url) || current_depth > max_depth) {
            active_tasks--;
            return;
        }
        visited_urls.insert(url);
    }

    try {
        bool is_https = (url.find("https://") == 0);
        std::string host, target;

        std::string temp_url = url;
        if (is_https) temp_url.erase(0, 8);
        else if (url.find("http://") == 0) temp_url.erase(0, 7);

        auto pos = temp_url.find('/');
        if (pos == std::string::npos) {
            host = temp_url; target = "/";
        }
        else {
            host = temp_url.substr(0, pos);
            target = temp_url.substr(pos);
        }

        std::string encoded_target = url_encode(target);

        std::string html = download(host, encoded_target, is_https);

        if (!html.empty()) {
            {
                std::lock_guard<std::mutex> lock(cout_mtx);
                std::cout << "[Thread " << std::this_thread::get_id() << "] Indexing: " << url << " (" << html.length() << " bytes)" << std::endl;
            }

            auto word_freq = indexer.process(html);

            if (!word_freq.empty()) {
                std::lock_guard<std::mutex> lock(db_mtx);
                db.save_indexing_result(url, word_freq);
            }

            if (current_depth < max_depth) {
                std::vector<std::string> links = extract_links(html);

                for (std::string& link : links) {
                    auto hash_pos = link.find('#');
                    if (hash_pos != std::string::npos) link = link.substr(0, hash_pos);

                    if (link.find("/") == 0 && link.find("//") != 0) {
                        link = (is_https ? "https://" : "http://") + host + link;
                    }
                    else if (link.find("//") == 0) {
                        link = (is_https ? "https:" : "http:") + link;
                    }

                    if (!is_useful_link(link)) continue;
                    active_tasks++;
                    boost::asio::post(pool, [this, link, current_depth, max_depth]() {
                        process_url(link, current_depth + 1, max_depth);
                        });
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(cout_mtx);
        std::cerr << "  [Error] Failed to process " << url << ": " << e.what() << std::endl;
    }
    catch (...) {
        std::lock_guard<std::mutex> lock(cout_mtx);
        std::cerr << "  [Error] Unknown error in process_url for " << url << std::endl;
    }

    active_tasks--;
}

std::string Spider::download(const std::string& host, const std::string& target, bool is_https) {
    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);

        if (is_https) {
            ssl::context ctx(ssl::context::tlsv12_client);

            ctx.set_verify_mode(ssl::verify_none);

            beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

            if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
                return "";
            }

            auto const results = resolver.resolve(host, "443");
            beast::get_lowest_layer(stream).connect(results);
            stream.handshake(ssl::stream_base::client);

            http::request<http::string_body> req{ http::verb::get, target, 11 };
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
            req.set(http::field::accept_encoding, "identity");

            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            std::cout << "HTTP Status: " << res.result_int() << " for " << host << target << std::endl;

            if (res.result_int() != 200) {
                return "";
            }


            beast::error_code ec;
            stream.shutdown(ec);

            return res.body();
        }
        else {
            beast::tcp_stream stream(ioc);
            auto const results = resolver.resolve(host, "80");
            stream.connect(results);

            http::request<http::string_body> req{ http::verb::get, target, 11 };
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "Mozilla/5.0");
            req.set(http::field::accept_encoding, "identity"); // Запрещаем сжатие
            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            return res.body();
        }
    }
    catch (std::exception const& e) {
        return "";
    }
}

std::vector<std::string> Spider::extract_links(const std::string& html) {
    std::vector<std::string> links;
    std::regex link_regex(R"(href=["']([^"']*)["'])", std::regex::icase);

    auto words_begin = std::sregex_iterator(html.begin(), html.end(), link_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::string link = (*i)[1].str();
        if (!link.empty()) {
            links.push_back(link);
        }
    }
    return links;
}
bool Spider::is_useful_link(const std::string& url) {
    if (url.find("mailto:") == 0 || url.find("tel:") == 0 || url.find("javascript:") == 0) {
        return false;
    }

    static const std::vector<std::string> blacklisted_ext = {
        ".jpg", ".jpeg", ".png", ".gif", ".pdf", ".zip", ".rar", ".exe", ".docx", ".mp3", ".mp4"
    };
    for (const auto& ext : blacklisted_ext) {
        if (url.size() >= ext.size() &&
            url.compare(url.size() - ext.size(), ext.size(), ext) == 0) return false;
    }

    static const std::vector<std::string> blacklisted_keywords = {
        "adserver", "banner", "pixel", "marketing", "click"
        "login", "signup", "logout", "auth", "profile",    
        "settings", "config", "theme", "lang=", "set_lang",   
        "action=edit", "action=history", "printable=yes",  
        "contact", "support", "help", "privacy", "terms"      
    };

    std::string url_lower = url;
    std::transform(url_lower.begin(), url_lower.end(), url_lower.begin(), ::tolower);

    for (const auto& word : blacklisted_keywords) {
        if (url_lower.find(word) != std::string::npos) return false;
    }

    if (url.find("https://") == 0) {
        if (url.find("https://ru.wikipedia.org") != 0 &&
            url.find("https://en.wikipedia.org") != 0) {
            return false;
        }
    }

    if (url.find("/wiki/") != std::string::npos) {
        std::string path = url.substr(url.find("/wiki/") + 6);
        if (path.find(":") != std::string::npos) return false;
    }

    return true;
}

std::string Spider::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' ||
            c == '/' || c == '?' || c == '=' || c == '&' || c == ':') {
            escaped << (char)c;
        }
        else {
            escaped << std::uppercase << '%' << std::setw(2) << int(c);
        }
    }
    return escaped.str();
}