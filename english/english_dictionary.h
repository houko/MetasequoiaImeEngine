#pragma once

#include "../core/word_item.h"
#include <cstddef>
#include <sqlite3.h>
#include <string>
#include <vector>

class EnglishDictionary
{
  public:
    explicit EnglishDictionary(std::string db_path);
    ~EnglishDictionary();

    EnglishDictionary(const EnglishDictionary &) = delete;
    EnglishDictionary &operator=(const EnglishDictionary &) = delete;

    std::vector<WordItem> query_prefix(const std::string &prefix, size_t limit = 5);
    bool ready();
    static bool ensure_schema(const std::string &db_path);

  private:
    bool ensure_query_statement();
    void close_database();

  private:
    std::string db_path_;
    sqlite3 *db_ = nullptr;
    sqlite3_stmt *query_statement_ = nullptr;
};
