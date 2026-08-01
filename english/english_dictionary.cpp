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

    if (!has_table)
    {
        const bool created = sqlite3_exec(database,
                                          "CREATE TABLE english_words("
                                          "word TEXT COLLATE BINARY NOT NULL,display TEXT NOT NULL,"
                                          "weight INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(word,display)) WITHOUT ROWID;"
                                          "PRAGMA user_version=2",
                                          nullptr, nullptr, nullptr) == SQLITE_OK;
        sqlite3_close(database);
        return created;
    }

    if (has_weight && composite_primary_key)
    {
        sqlite3_close(database);
        return true;
    }

    const char *copy_sql = has_weight ? "INSERT OR IGNORE INTO english_words_new(word,display,weight) "
                                        "SELECT word,display,weight FROM english_words;"
                                      : "INSERT OR IGNORE INTO english_words_new(word,display,weight) "
                                        "SELECT word,display,0 FROM english_words;";
    const bool ok = sqlite3_exec(database, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database,
                                 "CREATE TABLE english_words_new("
                                 "word TEXT COLLATE BINARY NOT NULL,display TEXT NOT NULL,"
                                 "weight INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(word,display)) WITHOUT ROWID",
                                 nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database, copy_sql, nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database, "DROP TABLE english_words", nullptr, nullptr, nullptr) == SQLITE_OK &&
                    sqlite3_exec(database, "ALTER TABLE english_words_new RENAME TO english_words", nullptr, nullptr,
                                 nullptr) == SQLITE_OK &&
                    sqlite3_exec(database, "PRAGMA user_version=2", nullptr, nullptr, nullptr) == SQLITE_OK;
    sqlite3_exec(database, ok ? "COMMIT" : "ROLLBACK", nullptr, nullptr, nullptr);
    sqlite3_close(database);
    return ok;
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

void EnglishDictionary::close_database()
{
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
