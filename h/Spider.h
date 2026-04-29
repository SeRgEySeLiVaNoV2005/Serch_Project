#pragma once
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <atomic>
#include <boost/asio/thread_pool.hpp>
#include "Database.h"
#include "Indexer.h"

class Spider {
public:
    Spider(Database& db, int num_threads = 4);

    void run(const std::string& start_url, int max_depth);

private:
    void process_url(std::string url, int current_depth, int max_depth);

    std::string download(const std::string& host, const std::string& target, bool is_https, int redirect_limit = 5);
    std::vector<std::string> extract_links(const std::string& html);
    bool is_useful_link(const std::string& url);
    std::string url_encode(const std::string& value);
    Database& db;
    Indexer indexer;

    std::set<std::string> visited_urls;
    std::mutex visited_mtx; 
    std::mutex db_mtx;      
    std::mutex cout_mtx;    

    boost::asio::thread_pool pool;
    std::atomic<int> active_tasks{ 0 }; 
};