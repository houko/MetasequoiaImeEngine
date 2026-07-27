#pragma once

#include <string>
#include <cstdint>
#include <utility>

enum class CandidateSource
{
    Database,
    UserDatabase,
    CloudSuggestion,
    AiSuggestion,
    EnglishDictionary,
    QuickPhrase,
    Generated,
    Fallback,
};

struct WordItem
{
    std::string pinyin;
    std::string word;
    std::int64_t weight = 0;
    CandidateSource source = CandidateSource::Database;
    int fixed_position = 0;

    WordItem() = default;
    WordItem(std::string pinyin_value,
             std::string word_value,
             std::int64_t weight_value,
             CandidateSource source_value = CandidateSource::Database)
        : pinyin(std::move(pinyin_value)), word(std::move(word_value)), weight(weight_value), source(source_value)
    {
    }
};
