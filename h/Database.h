#pragma once
#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <vector>

struct SearchResult {
    std::string url;
    int score;
};

class Database {
public:
    Database(const std::string& conn_str);
    void create_tables();
    void save_indexing_result(const std::string& url, const std::map<std::string, int>& word_freq);
    std::vector<SearchResult> search(const std::vector<std::string>& query_words);
private:
    std::unique_ptr<pqxx::connection> conn;
};