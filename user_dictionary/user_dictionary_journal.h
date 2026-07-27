#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include "../core/word_item.h"

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
                   const std::string &value, std::int64_t weight, const std::string &display = {});
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

bool adjust_candidate_ranking(const std::string &main_db_path, const std::string &user_db_path,
                              const std::string &context_key, const std::vector<WordItem> &ordered_candidates,
                              const std::string &entry_key, const std::string &value,
                              const std::string &mode, int linear_step, int trigger_count, bool force_top,
                              bool *ranking_changed = nullptr);
bool set_fixed_position(const std::string &user_db_path, const std::string &context_key,
                        const std::string &entry_key, const std::string &value, int position);
bool clear_fixed_position(const std::string &user_db_path, const std::string &context_key,
                          const std::string &entry_key, const std::string &value);
bool is_fixed(const std::string &user_db_path, const std::string &context_key,
              const std::string &entry_key, const std::string &value);
void apply_fixed_positions(
    const std::string &user_db_path, const std::string &context_key,
    std::vector<WordItem> &candidates, bool include_missing,
    const std::function<std::optional<WordItem>(const std::string &, const std::string &)> &find_candidate = {});
} // namespace user_dictionary
