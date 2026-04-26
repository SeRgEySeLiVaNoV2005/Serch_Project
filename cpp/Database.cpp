#include "Database.h"
#include <iostream>

Database::Database(const std::string& conn_str) {
    conn = std::make_unique<pqxx::connection>(conn_str);
    pqxx::work W(*conn);
    W.exec0("SET client_encoding TO 'UTF8'");
    W.commit();

    if (conn->is_open()) {
        std::cout << "Успешное подключение к БД: " << conn->dbname() << std::endl;
    }
    else {
        throw std::runtime_error("Не удалось подключиться к БД");
    }
}

void Database::create_tables() {
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS documents (
            id SERIAL PRIMARY KEY,
            url TEXT UNIQUE NOT NULL
        );

        CREATE TABLE IF NOT EXISTS words (
            id SERIAL PRIMARY KEY,
            word VARCHAR(100) UNIQUE NOT NULL
        );

        CREATE TABLE IF NOT EXISTS index (
            word_id INTEGER REFERENCES words(id) ON DELETE CASCADE,
            document_id INTEGER REFERENCES documents(id) ON DELETE CASCADE,
            count INTEGER NOT NULL,
            PRIMARY KEY (word_id, document_id)
        );
    )";

    pqxx::work W(*conn);
    W.exec(sql);
    W.commit();

    std::cout << "Таблицы успешно созданы/проверены." << std::endl;
}

void Database::save_indexing_result(const std::string& url, const std::map<std::string, int>& word_freq) {
    pqxx::work W(*conn);

    auto res_doc = W.exec_params(
        "INSERT INTO documents (url) VALUES ($1) ON CONFLICT (url) DO UPDATE SET url=EXCLUDED.url RETURNING id",
        url
    );
    int doc_id = res_doc[0][0].as<int>();

    for (const auto& [word, count] : word_freq) {
        auto res_word = W.exec_params(
            "INSERT INTO words (word) VALUES ($1) ON CONFLICT (word) DO UPDATE SET word=EXCLUDED.word RETURNING id",
            word
        );
        int word_id = res_word[0][0].as<int>();

        W.exec_params(
            "INSERT INTO index (word_id, document_id, count) VALUES ($1, $2, $3) "
            "ON CONFLICT (word_id, document_id) DO UPDATE SET count = EXCLUDED.count",
            word_id, doc_id, count
        );
    }

    W.commit();
}

std::vector<SearchResult> Database::search(const std::vector<std::string>& query_words) {
    if (query_words.empty()) return {};

    pqxx::work W(*conn);

    std::string word_list;
    for (size_t i = 0; i < query_words.size(); ++i) {
        word_list += W.quote(query_words[i]);
        if (i < query_words.size() - 1) word_list += ",";
    }

    std::string sql = R"(
        SELECT d.url, SUM(i.count) as total_relevance
        FROM documents d
        JOIN index i ON d.id = i.document_id
        JOIN words w ON i.word_id = w.id
        WHERE w.word IN ()" + word_list + R"()
        GROUP BY d.id, d.url
        HAVING COUNT(DISTINCT w.word) = )" + std::to_string(query_words.size()) + R"(
        ORDER BY total_relevance DESC
        LIMIT 10;
    )";

    std::vector<SearchResult> results;
    pqxx::result res = W.exec(sql);

    for (const auto& row : res) {
        results.push_back({ row[0].as<std::string>(), row[1].as<int>() });
    }

    return results;
}