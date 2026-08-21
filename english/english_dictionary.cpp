#include "english_dictionary.h"

#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>
#include <utility>

namespace
{
bool IsLowerAsciiWord(const std::string &value)
{
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) { return ch >= 'a' && ch <= 'z'; });
}
} // namespace

EnglishDictionary::EnglishDictionary(std::string db_path) : db_path_(std::move(db_path))
{
    (void)ensure_schema(db_path_);
}

EnglishDictionary::~EnglishDictionary()
{
    close_database();
}

std::vector<WordItem> EnglishDictionary::query_prefix(const std::string &prefix, size_t limit)
{
    if (!IsLowerAsciiWord(prefix) || limit == 0 || !ensure_query_statement())
    {
        return {};
    }

    const std::string upper_bound = prefix + "{";
    const int sqlite_limit =
        static_cast<int>((std::min)(limit, static_cast<size_t>((std::numeric_limits<int>::max)())));

    sqlite3_reset(query_statement_);
    sqlite3_clear_bindings(query_statement_);
    if (sqlite3_bind_text(query_statement_, 1, prefix.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(query_statement_, 2, upper_bound.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(query_statement_, 3, sqlite_limit) != SQLITE_OK)
    {
        return {};
    }

    std::vector<WordItem> candidates;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(query_statement_)) == SQLITE_ROW)
    {
        const auto *word = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 0));
        const auto *display = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 1));
        if (word == nullptr || display == nullptr)
        {
            continue;
        }
        candidates.emplace_back(word, display, sqlite3_column_int64(query_statement_, 2),
                                CandidateSource::EnglishDictionary);
    }

    if (result != SQLITE_DONE)
    {
        (void)0;
        return {};
    }
    return candidates;
}

bool EnglishDictionary::ready()
{
    return ensure_query_statement();
}

namespace
{
std::string QueryGloss(sqlite3_stmt *statement, const std::string &key)
{
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
    if (sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        return {};
    std::string result;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));
        if (value != nullptr)
            result = value;
    }
    sqlite3_reset(statement);
    return result;
}
} // namespace

std::string EnglishDictionary::query_chinese_gloss(const std::string &english)
{
    return english.empty() || !ensure_gloss_statements() ? std::string{} : QueryGloss(en_zh_statement_, english);
}

std::string EnglishDictionary::query_english_gloss(const std::string &chinese)
{
    return chinese.empty() || !ensure_gloss_statements() ? std::string{} : QueryGloss(zh_en_statement_, chinese);
}

bool EnglishDictionary::upsert_gloss(const std::string &db_path, bool chinese_to_english, const std::string &key,
                                     const std::string &gloss)
{
    if (db_path.empty() || key.empty() || gloss.empty() || !ensure_schema(db_path))
        return false;

    sqlite3 *database = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
    {
        if (database != nullptr)
            sqlite3_close(database);
        return false;
    }
    sqlite3_busy_timeout(database, 250);
    sqlite3_stmt *statement = nullptr;
    const char *sql = chinese_to_english
                          ? "INSERT OR REPLACE INTO zh_en_glosses(chinese,english_gloss) VALUES(?1,?2)"
                          : "INSERT OR REPLACE INTO en_zh_glosses(english,chinese_gloss) VALUES(?1,?2)";
    bool ok = sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK &&
              sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
              sqlite3_bind_text(statement, 2, gloss.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
              sqlite3_step(statement) == SQLITE_DONE;
    if (statement != nullptr)
        sqlite3_finalize(statement);
    sqlite3_close(database);
    return ok;
}

bool EnglishDictionary::ensure_schema(const std::string &db_path)
{
    sqlite3 *database = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
    {
        if (database != nullptr)
            sqlite3_close(database);
        return false;
    }

    bool has_weight = false;
    bool composite_primary_key = false;
    bool has_table = false;
    sqlite3_stmt *columns = nullptr;
    if (sqlite3_prepare_v2(database, "PRAGMA table_info(english_words)", -1, &columns, nullptr) == SQLITE_OK)
    {
        int primary_key_columns = 0;
        while (sqlite3_step(columns) == SQLITE_ROW)
        {
            has_table = true;
            const auto *name = reinterpret_cast<const char *>(sqlite3_column_text(columns, 1));
            has_weight = has_weight || (name != nullptr && std::string(name) == "weight");
            if (sqlite3_column_int(columns, 5) > 0)
                ++primary_key_columns;
        }
        composite_primary_key = primary_key_columns == 2;
    }
    if (columns != nullptr)
        sqlite3_finalize(columns);

    bool english_words_ok = false;
    if (!has_table)
    {
        english_words_ok = sqlite3_exec(database,
                                          "CREATE TABLE english_words("
                                          "word TEXT COLLATE BINARY NOT NULL,display TEXT NOT NULL,"
                                          "weight INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(word,display)) WITHOUT ROWID;",
                                          nullptr, nullptr, nullptr) == SQLITE_OK;
    }
    else if (has_weight && composite_primary_key)
    {
        english_words_ok = true;
    }
    else
    {
        const char *copy_sql = has_weight ? "INSERT OR IGNORE INTO english_words_new(word,display,weight) "
                                        "SELECT word,display,weight FROM english_words;"
                                      : "INSERT OR IGNORE INTO english_words_new(word,display,weight) "
                                        "SELECT word,display,0 FROM english_words;";
        english_words_ok = sqlite3_exec(database, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database,
                                 "CREATE TABLE english_words_new("
                                 "word TEXT COLLATE BINARY NOT NULL,display TEXT NOT NULL,"
                                 "weight INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(word,display)) WITHOUT ROWID",
                                 nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database, copy_sql, nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database, "DROP TABLE english_words", nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database, "ALTER TABLE english_words_new RENAME TO english_words", nullptr, nullptr,
                                 nullptr) == SQLITE_OK;
        sqlite3_exec(database, english_words_ok ? "COMMIT" : "ROLLBACK", nullptr, nullptr, nullptr);
    }

    const bool gloss_tables_ok = english_words_ok &&
        sqlite3_exec(database,
                     "CREATE TABLE IF NOT EXISTS en_zh_glosses("
                     "english TEXT COLLATE BINARY PRIMARY KEY,chinese_gloss TEXT NOT NULL) WITHOUT ROWID;"
                     "CREATE TABLE IF NOT EXISTS zh_en_glosses("
                     "chinese TEXT COLLATE BINARY PRIMARY KEY,english_gloss TEXT NOT NULL) WITHOUT ROWID;"
                     "PRAGMA user_version=3;",
                     nullptr, nullptr, nullptr) == SQLITE_OK;
    sqlite3_close(database);
    return gloss_tables_ok;
}

bool EnglishDictionary::ensure_query_statement()
{
    if (query_statement_ != nullptr)
    {
        return true;
    }

    if (db_ == nullptr &&
        sqlite3_open_v2(db_path_.c_str(), &db_, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
    {
        (void)0;
        close_database();
        return false;
    }

    constexpr const char *query_sql =
        "SELECT word,display,weight FROM english_words "
        "WHERE word >= ?1 AND word < ?2 "
        "ORDER BY CASE WHEN word = ?1 THEN 0 ELSE 1 END, weight DESC, length(word), word, display "
        "LIMIT ?3";
    if (sqlite3_prepare_v2(db_, query_sql, -1, &query_statement_, nullptr) != SQLITE_OK)
    {
        (void)0;
        close_database();
        return false;
    }
    return true;
}

bool EnglishDictionary::ensure_gloss_statements()
{
    if (en_zh_statement_ != nullptr && zh_en_statement_ != nullptr)
        return true;
    if (!ensure_query_statement())
        return false;
    if (sqlite3_prepare_v2(db_, "SELECT chinese_gloss FROM en_zh_glosses WHERE english=?1", -1,
                           &en_zh_statement_, nullptr) != SQLITE_OK ||
        sqlite3_prepare_v2(db_, "SELECT english_gloss FROM zh_en_glosses WHERE chinese=?1", -1,
                           &zh_en_statement_, nullptr) != SQLITE_OK)
    {
        close_database();
        return false;
    }
    return true;
}

void EnglishDictionary::close_database()
{
    if (en_zh_statement_ != nullptr)
    {
        sqlite3_finalize(en_zh_statement_);
        en_zh_statement_ = nullptr;
    }
    if (zh_en_statement_ != nullptr)
    {
        sqlite3_finalize(zh_en_statement_);
        zh_en_statement_ = nullptr;
    }
    if (query_statement_ != nullptr)
    {
        sqlite3_finalize(query_statement_);
        query_statement_ = nullptr;
    }
    if (db_ != nullptr)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}
