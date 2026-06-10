#include "engine.h"

#include <fmt/base.h>

QuanpinEngine::QuanpinEngine() = default;

QuanpinEngine::~QuanpinEngine() = default;

std::vector<WordItem> QuanpinEngine::query(const QueryRequest &request)
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

int QuanpinEngine::handleVkCode(UINT vk, UINT modifiers_down, WCHAR wch)
{
    return dictionary_.handleVkCode(vk, modifiers_down, wch);
}

int QuanpinEngine::create_word(std::string pinyin, std::string word)
{
    return dictionary_.create_word(std::move(pinyin), std::move(word));
}

int QuanpinEngine::update_weight_by_word(std::string word)
{
    return dictionary_.update_weight_by_word(std::move(word));
}

int QuanpinEngine::update_weight_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int QuanpinEngine::delete_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

std::string QuanpinEngine::search_sentence_from_ime_engine(const std::string &user_pinyin)
{
    return dictionary_.search_sentence_from_ime_engine(user_pinyin);
}

void QuanpinEngine::reset_state()
{
    dictionary_.reset_state();
}

void QuanpinEngine::reset_cache()
{
    dictionary_.reset_cache();
}
