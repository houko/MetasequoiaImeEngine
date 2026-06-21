#include "quanpin_query.h"

#include "quanpin_utils.h"
#include "../shuangpin/shuangpin_utils.h"
#include <algorithm>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace quanpin
{
namespace
{

std::vector<std::string> split(const std::string &text, char delimiter)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (true)
    {
        const size_t pos = text.find(delimiter, start);
        if (pos == std::string::npos)
        {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
}

std::string build_table_name_impl(const Segments &segments)
{
    if (segments.empty() || segments.front().empty())
    {
        return "";
    }
    if (segments.size() >= 8)
    {
        return "tbl_others_" + std::string(1, segments.front().front());
    }
    return "tbl_" + std::to_string(segments.size()) + "_" + std::string(1, segments.front().front());
}

std::string segments_to_jianpin_impl(const Segments &segments)
{
    std::string jp;
    for (const auto &segment : segments)
    {
        if (!segment.empty())
        {
            jp.push_back(segment.front());
        }
    }
    return jp;
}

std::string build_key_like_pattern(const Segments &segments)
{
    Segments parts;
    for (size_t i = 0; i < segments.size(); ++i)
    {
        const bool is_last = (i + 1 == segments.size());
        if (is_last || segments[i].size() == 1)
        {
            parts.push_back(segments[i] + "%");
        }
        else
        {
            parts.push_back(segments[i]);
        }
    }
    return join_segments(parts);
}

std::string build_key_prefix_upper_bound(const std::string &prefix)
{
    return prefix + "{";
}

bool is_pure_jianpin(const Segments &segments)
{
    return std::all_of(segments.begin(), segments.end(),
                       [](const std::string &segment) { return segment.size() == 1; });
}

bool can_match_exact_key(const Segments &segments)
{
    if (segments.empty())
    {
        return false;
    }

    const auto &valid_pinyin = intact_pinyin_set();
    return std::all_of(segments.begin(), segments.end(),
                       [&](const std::string &segment) { return valid_pinyin.find(segment) != valid_pinyin.end(); });
}

class SqliteDb
{
  public:
    explicit SqliteDb(const std::string &db_path)
    {
        if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK)
        {
            const std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "sqlite open failed";
            if (db_ != nullptr)
            {
                sqlite3_close(db_);
                db_ = nullptr;
            }
            throw std::runtime_error(message);
        }
    }

    ~SqliteDb()
    {
        if (db_ != nullptr)
        {
            sqlite3_close(db_);
        }
    }

    sqlite3 *get() const
    {
        return db_;
    }

  private:
    sqlite3 *db_ = nullptr;
};

std::vector<QueryItem> run_query(sqlite3 *db, const std::string &sql, const std::string &value, int limit)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        return {};
    }

    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    std::vector<QueryItem> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        const int weight = sqlite3_column_int(stmt, 1);
        rows.emplace_back(text == nullptr ? "" : reinterpret_cast<const char *>(text), weight);
    }
    sqlite3_finalize(stmt);
    return rows;
}

std::vector<QueryItem> run_query(sqlite3 *db,
                                 std::unordered_map<std::string, sqlite3_stmt *> &statement_cache,
                                 const std::string &sql,
                                 const std::string &value,
                                 int limit)
{
    sqlite3_stmt *stmt = nullptr;
    const auto found = statement_cache.find(sql);
    if (found != statement_cache.end())
    {
        stmt = found->second;
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    else
    {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            return {};
        }
        statement_cache.emplace(sql, stmt);
    }

    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    std::vector<QueryItem> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        const int weight = sqlite3_column_int(stmt, 1);
        rows.emplace_back(text == nullptr ? "" : reinterpret_cast<const char *>(text), weight);
    }
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return rows;
}

std::vector<QueryItem> run_query(sqlite3 *db,
                                 const std::string &sql,
                                 const std::string &lower_bound,
                                 const std::string &upper_bound,
                                 int limit)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        return {};
    }

    sqlite3_bind_text(stmt, 1, lower_bound.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, upper_bound.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, limit);

    std::vector<QueryItem> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        const int weight = sqlite3_column_int(stmt, 1);
        rows.emplace_back(text == nullptr ? "" : reinterpret_cast<const char *>(text), weight);
    }
    sqlite3_finalize(stmt);
    return rows;
}

std::vector<QueryItem> run_query(sqlite3 *db,
                                 std::unordered_map<std::string, sqlite3_stmt *> &statement_cache,
                                 const std::string &sql,
                                 const std::string &lower_bound,
                                 const std::string &upper_bound,
                                 int limit)
{
    sqlite3_stmt *stmt = nullptr;
    const auto found = statement_cache.find(sql);
    if (found != statement_cache.end())
    {
        stmt = found->second;
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    else
    {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            return {};
        }
        statement_cache.emplace(sql, stmt);
    }

    sqlite3_bind_text(stmt, 1, lower_bound.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, upper_bound.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, limit);

    std::vector<QueryItem> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        const int weight = sqlite3_column_int(stmt, 1);
        rows.emplace_back(text == nullptr ? "" : reinterpret_cast<const char *>(text), weight);
    }
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return rows;
}

std::vector<QueryItem> query_single_cut(sqlite3 *db, const Segments &segments, int limit)
{
    const auto table = build_table_name_impl(segments);
    if (table.empty())
    {
        return {};
    }

    const auto key = join_segments(segments);
    const auto jp = segments_to_jianpin_impl(segments);
    const auto key_prefix_pattern = build_key_like_pattern(segments);
    const auto key_prefix = key_prefix_pattern.substr(0, key_prefix_pattern.size() - 1);
    const auto key_prefix_upper_bound = build_key_prefix_upper_bound(key_prefix);

    const auto exact_sql =
        "SELECT \"value\", \"weight\" FROM \"" + table + "\" WHERE \"key\" = ? ORDER BY \"weight\" DESC LIMIT ?";
    std::vector<QueryItem> rows;
    if (can_match_exact_key(segments))
    {
        rows = run_query(db, exact_sql, key, limit);
        if (!rows.empty())
        {
            return rows;
        }
    }

    const auto prefix_sql = "SELECT \"value\", \"weight\" FROM \"" + table +
                            "\" WHERE \"key\" >= ? AND \"key\" < ? ORDER BY \"weight\" DESC LIMIT ?";
    rows = run_query(db, prefix_sql, key_prefix, key_prefix_upper_bound, limit);
    if (!rows.empty())
    {
        return rows;
    }

    if (!is_pure_jianpin(segments))
    {
        return {};
    }

    const auto jp_sql =
        "SELECT \"value\", \"weight\" FROM \"" + table + "\" WHERE \"jp\" = ? ORDER BY \"weight\" DESC LIMIT ?";
    return run_query(db, jp_sql, jp, limit);
}

std::vector<QueryItem> query_single_cut(sqlite3 *db,
                                        std::unordered_map<std::string, sqlite3_stmt *> &statement_cache,
                                        const Segments &segments,
                                        int limit)
{
    const auto table = build_table_name_impl(segments);
    if (table.empty())
    {
        return {};
    }

    const auto key = join_segments(segments);
    const auto jp = segments_to_jianpin_impl(segments);
    const auto key_prefix_pattern = build_key_like_pattern(segments);
    const auto key_prefix = key_prefix_pattern.substr(0, key_prefix_pattern.size() - 1);
    const auto key_prefix_upper_bound = build_key_prefix_upper_bound(key_prefix);

    const auto exact_sql =
        "SELECT \"value\", \"weight\" FROM \"" + table + "\" WHERE \"key\" = ? ORDER BY \"weight\" DESC LIMIT ?";
    std::vector<QueryItem> rows;
    if (can_match_exact_key(segments))
    {
        rows = run_query(db, statement_cache, exact_sql, key, limit);
        if (!rows.empty())
        {
            return rows;
        }
    }

    const auto prefix_sql = "SELECT \"value\", \"weight\" FROM \"" + table +
                            "\" WHERE \"key\" >= ? AND \"key\" < ? ORDER BY \"weight\" DESC LIMIT ?";
    rows = run_query(db, statement_cache, prefix_sql, key_prefix, key_prefix_upper_bound, limit);
    if (!rows.empty())
    {
        return rows;
    }

    if (!is_pure_jianpin(segments))
    {
        return {};
    }

    const auto jp_sql =
        "SELECT \"value\", \"weight\" FROM \"" + table + "\" WHERE \"jp\" = ? ORDER BY \"weight\" DESC LIMIT ?";
    return run_query(db, statement_cache, jp_sql, jp, limit);
}

} // namespace

Segments cut_pinyin_greedy(const std::string &pinyin, bool intact_only)
{
    if (pinyin.empty())
    {
        return {};
    }

    if (pinyin.find('\'') == std::string::npos)
    {
        return cut_one_piece_greedy(pinyin, intact_only);
    }

    Segments merged;
    for (const auto &part : split(pinyin, '\''))
    {
        auto cut = cut_one_piece_greedy(part, intact_only);
        if (cut.empty())
        {
            if (!part.empty())
            {
                merged.push_back(part);
            }
            continue;
        }
        merged.insert(merged.end(), cut.begin(), cut.end());
    }
    return merged;
}

std::vector<Segments> cut_pinyin_by_mode(const std::string &pinyin, const std::string &mode)
{
    if (mode != "greedy" && mode != "correction")
    {
        throw std::invalid_argument("mode must be one of: greedy, correction");
    }

    const auto greedy = cut_pinyin_greedy(pinyin, false);
    if (!greedy.empty())
    {
        return {greedy};
    }

    if (mode == "greedy")
    {
        return {};
    }

    const auto intact = cut_pinyin_greedy(pinyin, true);
    if (!intact.empty())
    {
        return {intact};
    }

    return {};
}

Segments split_segments(const std::string &segmentation)
{
    if (segmentation.empty())
    {
        return {};
    }

    return split(segmentation, '\'');
}

std::string join_segments(const Segments &segments, const std::string &delimiter)
{
    std::string joined;
    for (size_t i = 0; i < segments.size(); ++i)
    {
        if (i > 0)
        {
            joined += delimiter;
        }
        joined += segments[i];
    }
    return joined;
}

std::string build_table_name(const Segments &segments)
{
    return build_table_name_impl(segments);
}

std::string segments_to_jianpin(const Segments &segments)
{
    return segments_to_jianpin_impl(segments);
}

std::string get_default_db_path()
{
    return shuangpin::get_local_appdata_path() + "\\" + shuangpin::get_app_name() + "\\quanpin_multi_tbl_has_jp.db";
}

void warm_up(sqlite3 *db, std::unordered_map<std::string, sqlite3_stmt *> &statement_cache)
{
    (void)intact_pinyin_set();
    (void)prefix_pinyin_set();

    if (db == nullptr)
    {
        return;
    }

    // Warm the most common single-syllable prefixes to hide first-query setup costs.
    (void)query_single_cut(db, statement_cache, Segments{"n"}, 1);
    (void)query_single_cut(db, statement_cache, Segments{"ni"}, 1);
}

QueryResult query_words(const std::string &pinyin, const std::string &db_path, const std::string &mode, int limit)
{
    const auto cuts = cut_pinyin_by_mode(pinyin, mode);
    QueryResult result{pinyin, mode, {}};
    if (cuts.empty())
    {
        return result;
    }

    SqliteDb db(db_path);
    for (const auto &segments : cuts)
    {
        result.results.push_back(QueryResultEntry{
            segments,
            join_segments(segments),
            build_table_name(segments),
            query_single_cut(db.get(), segments, limit),
        });
    }
    return result;
}

QueryResult query_segments(const Segments &segments, const std::string &db_path, int limit)
{
    QueryResult result{join_segments(segments), "precut", {}};
    if (segments.empty())
    {
        return result;
    }

    SqliteDb db(db_path);
    result.results.push_back(QueryResultEntry{
        segments,
        join_segments(segments),
        build_table_name(segments),
        query_single_cut(db.get(), segments, limit),
    });
    return result;
}

std::vector<QueryItem> query_words_flat(const std::string &pinyin, const std::string &db_path, const std::string &mode,
                                        int limit)
{
    const auto result = query_words(pinyin, db_path, mode, limit);
    std::vector<QueryItem> items;
    std::unordered_set<std::string> seen;
    for (const auto &entry : result.results)
    {
        for (const auto &item : entry.items)
        {
            if (seen.insert(item.first).second)
            {
                items.push_back(item);
            }
        }
    }

    std::sort(items.begin(), items.end(), [](const QueryItem &lhs, const QueryItem &rhs) {
        return lhs.second > rhs.second;
    });

    if (static_cast<int>(items.size()) > limit)
    {
        items.resize(static_cast<size_t>(limit));
    }
    return items;
}

std::vector<QueryItem> query_segments_flat(const Segments &segments, const std::string &db_path, int limit)
{
    const auto result = query_segments(segments, db_path, limit);
    std::vector<QueryItem> items;
    std::unordered_set<std::string> seen;
    for (const auto &entry : result.results)
    {
        for (const auto &item : entry.items)
        {
            if (seen.insert(item.first).second)
            {
                items.push_back(item);
            }
        }
    }

    std::sort(items.begin(), items.end(), [](const QueryItem &lhs, const QueryItem &rhs) {
        return lhs.second > rhs.second;
    });

    if (static_cast<int>(items.size()) > limit)
    {
        items.resize(static_cast<size_t>(limit));
    }
    return items;
}

std::vector<QueryItem> query_segments_flat(const Segments &segments,
                                           sqlite3 *db,
                                           std::unordered_map<std::string, sqlite3_stmt *> &statement_cache,
                                           int limit)
{
    if (db == nullptr || segments.empty())
    {
        return {};
    }

    std::vector<QueryItem> items;
    std::unordered_set<std::string> seen;
    for (const auto &item : query_single_cut(db, statement_cache, segments, limit))
    {
        if (seen.insert(item.first).second)
        {
            items.push_back(item);
        }
    }

    std::sort(items.begin(), items.end(), [](const QueryItem &lhs, const QueryItem &rhs) {
        return lhs.second > rhs.second;
    });

    if (static_cast<int>(items.size()) > limit)
    {
        items.resize(static_cast<size_t>(limit));
    }
    return items;
}

} // namespace quanpin
