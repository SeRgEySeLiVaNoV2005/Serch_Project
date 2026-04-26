#pragma once
#include <string>
#include <map>
#include <vector>

class Indexer {
public:
    std::map<std::string, int> process(const std::string& html);

private:
    std::vector<std::string> clean_and_tokenize(const std::string& text);
};