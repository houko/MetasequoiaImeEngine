#pragma once

#include <string>

namespace user_dictionary
{
enum class DictionaryKind
{
    Pinyin,
    Wubi,
    QuickPhrase,
    English,
};

std::string default_user_db_path();

bool record_upsert(const std::string &user_db_path, DictionaryKind kind, const std::string &key,
                   const std::string &value, int weight, const std::string &display = {});
bool record_delete(const std::string &user_db_path, DictionaryKind kind, const std::string &key,
                   const std::string &value);
bool record_pinyin_upsert_from_database(const std::string &main_db_path, const std::string &key,
                                        const std::string &value);

struct ReplayResult
{
    int applied = 0;
    int failed = 0;
    std::string error;
};

ReplayResult replay(const std::string &user_db_path, const std::string &main_db_path,
                    const std::string &english_db_path);
} // namespace user_dictionary
