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
    const int sqlite_limit = static_cast<int>((std::min)(limit, static_cast<size_t>((std::numeric_limits<int>::max)())));

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
        const auto *display = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 0));
        if (display == nullptr)
        {
            continue;
        }
        candidates.emplace_back(prefix, display, 0, CandidateSource::EnglishDictionary);
    }

    if (result != SQLITE_DONE)
    {
        spdlog::warn("English prefix query failed for '{}': {}", prefix, sqlite3_errmsg(db_));
        return {};
    }
    return candidates;
}

bool EnglishDictionary::ready()
{
    return ensure_query_statement();
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
        spdlog::warn("Unable to open English database '{}': {}", db_path_,
                     db_ == nullptr ? "unknown error" : sqlite3_errmsg(db_));
        close_database();
        return false;
    }

    constexpr const char *query_sql =
        "SELECT display FROM english_words "
        "WHERE word >= ?1 AND word < ?2 "
        "ORDER BY CASE WHEN word = ?1 THEN 0 ELSE 1 END, length(word), word "
        "LIMIT ?3";
    if (sqlite3_prepare_v2(db_, query_sql, -1, &query_statement_, nullptr) != SQLITE_OK)
    {
        spdlog::warn("Unable to prepare English prefix query for '{}': {}", db_path_, sqlite3_errmsg(db_));
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
