#include "engine.h"

#include <fmt/base.h>

std::vector<WordItem> ShuangpinEngine::query(const QueryRequest &request)
{
    if (!request.valid)
    {
        return {};
    }

    dictionary_.reset_state();
    for (const auto &key_stroke : request.key_strokes)
    {
        fmt::println("Handling keystroke: vk={}", key_stroke.vk);
        dictionary_.handleVkCode(key_stroke.vk, key_stroke.modifiers_down, key_stroke.wch);
    }
    fmt::println("length: {}", dictionary_.get_current_candidate_list().size());
    return dictionary_.get_current_candidate_list();
}

std::string ShuangpinEngine::search_sentence_from_ime_engine(const std::string &user_pinyin)
{
    return dictionary_.search_sentence_from_ime_engine(user_pinyin);
}

void ShuangpinEngine::reset_cache()
{
    dictionary_.reset_cache();
}
