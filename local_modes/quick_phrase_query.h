#pragma once

#include "../core/word_item.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace metasequoia::local_modes
{
struct QuickPhraseQueryResult
{
    std::vector<WordItem> candidates;
    std::optional<std::string> diagnostic;
};

QuickPhraseQueryResult query_quick_phrases(const std::string &prefix, int limit = 100);
QuickPhraseQueryResult query_quick_phrases(const std::string &prefix,
                                           const std::filesystem::path &database_path,
                                           int limit = 100);
} // namespace metasequoia::local_modes
