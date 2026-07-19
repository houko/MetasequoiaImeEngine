#pragma once

#include <string>
#include <utility>

enum class CandidateSource
{
    Database,
    UserDatabase,
    CloudSuggestion,
    EnglishDictionary,
    QuickPhrase,
    Generated,
    Fallback,
};

struct WordItem
{
    std::string pinyin;
    std::string word;
    int weight = 0;
    CandidateSource source = CandidateSource::Database;

    WordItem() = default;
    WordItem(std::string pinyin_value,
             std::string word_value,
             int weight_value,
             CandidateSource source_value = CandidateSource::Database)
        : pinyin(std::move(pinyin_value)), word(std::move(word_value)), weight(weight_value), source(source_value)
    {
    }
};
