#pragma once

#include <string>
#include <utility>
#include <vector>

namespace quanpin
{

using Segments = std::vector<std::string>;
using QueryItem = std::pair<std::string, int>;

struct QueryResultEntry
{
    Segments segments;
    std::string key;
    std::string table;
    std::vector<QueryItem> items;
};

struct QueryResult
{
    std::string pinyin;
    std::string mode;
    std::vector<QueryResultEntry> results;
};

Segments cut_pinyin_greedy(const std::string &pinyin, bool intact_only = false);
std::vector<Segments> cut_pinyin_by_mode(const std::string &pinyin, const std::string &mode = "greedy");
std::string join_segments(const Segments &segments, const std::string &delimiter = "'");
std::string build_table_name(const Segments &segments);
std::string segments_to_jianpin(const Segments &segments);
std::string get_default_db_path();
QueryResult query_words(const std::string &pinyin, const std::string &db_path, const std::string &mode = "greedy",
                        int limit = 8);

} // namespace quanpin
