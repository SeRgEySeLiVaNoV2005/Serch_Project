#include "Indexer.h"
#include <boost/locale.hpp>
#include <iostream>
#include <algorithm>

std::map<std::string, int> Indexer::process(const std::string& html) {
    if (html.empty()) return {};
    std::string plain_text;
    plain_text.reserve(html.size());
    bool in_tag = false;
    for (unsigned char c : html) {
        if (c == '<') in_tag = true;
        else if (c == '>') {
            in_tag = false;
            plain_text += ' '; 
        }
        else if (!in_tag) {
            plain_text += c;
        }
    }

    boost::locale::generator gen;
    std::locale loc = gen("en_US.UTF-8");
    std::string lower_text = boost::locale::to_lower(plain_text, loc);

    std::map<std::string, int> freq_map;
    std::string current_word;

    for (size_t i = 0; i < lower_text.length(); ++i) {
        unsigned char c = lower_text[i];

        if ((c >= 'a' && c <= 'z') || (c >= 0x80)) {
            current_word += c;
        }
        else {
            if (!current_word.empty()) {
                if (current_word.length() >= 3 && current_word.length() <= 64) {
                    unsigned char first = static_cast<unsigned char>(current_word[0]);
                    if ((first >= 'a' && first <= 'z') || first == 0xd0 || first == 0xd1) {
                        freq_map[current_word]++;
                    }
                }
                current_word.clear();
            }
        }
    }

    if (current_word.length() >= 3 && current_word.length() <= 64) {
        unsigned char first = static_cast<unsigned char>(current_word[0]);
        if ((first >= 'a' && first <= 'z') || first == 0xd0 || first == 0xd1) {
            freq_map[current_word]++;
        }
    }

    return freq_map;
}

std::vector<std::string> Indexer::clean_and_tokenize(const std::string& text) {
    namespace bl = boost::locale;
    bl::generator gen;
    std::locale loc = gen("en_US.UTF-8");

    std::string lower_text = bl::to_lower(text, loc);
    std::vector<std::string> words;
    std::string current_word;

    for (unsigned char c : lower_text) {
        if (std::isalpha(c) || (c > 127)) { 
            current_word += c;
        }
        else {
            if (!current_word.empty()) {
                words.push_back(current_word);
                current_word.clear();
            }
        }
    }
    if (!current_word.empty()) words.push_back(current_word);

    return words;
}