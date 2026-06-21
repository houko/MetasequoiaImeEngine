#include "engine.h"
#include "../common/helpcode_utils.h"
#include "quanpin_query.h"

QuanpinEngine::QuanpinEngine() = default;

QuanpinEngine::~QuanpinEngine() = default;

std::vector<WordItem> QuanpinEngine::query(const QueryRequest &request)
{
    if (!request.valid)
    {
        return {};
    }

    if (request.enable_shuangpin_helpcode && HelpcodeUtils::is_quanpin_double_help_mode(request.raw_input_with_cases) &&
        request.raw_input.size() >= 2)
    {
        const std::string base_raw_input = request.raw_input.substr(0, request.raw_input.size() - 2);
        if (quanpin::is_complete_pinyin_input(base_raw_input))
        {
            const auto cuts = quanpin::cut_pinyin_by_mode(base_raw_input, "correction");
            const std::string base_segmentation = quanpin::join_segments(cuts.front());
            const std::string help_codes = request.raw_input.substr(request.raw_input.size() - 2, 2);
            const auto base_candidates = dictionary_.query(base_raw_input, base_segmentation);
            return HelpcodeUtils::filter_candidates_with_double_helpcodes(base_candidates, help_codes);
        }
    }

    if (request.enable_shuangpin_helpcode && HelpcodeUtils::is_quanpin_single_help_mode(request.raw_input_with_cases) &&
        !request.raw_input.empty())
    {
        const std::string base_raw_input = request.raw_input.substr(0, request.raw_input.size() - 1);
        if (quanpin::is_complete_pinyin_input(base_raw_input))
        {
            const auto cuts = quanpin::cut_pinyin_by_mode(base_raw_input, "correction");
            const std::string base_segmentation = quanpin::join_segments(cuts.front());
            const std::string help_code = request.raw_input.substr(request.raw_input.size() - 1, 1);
            const auto base_candidates = dictionary_.query(base_raw_input, base_segmentation);
            return HelpcodeUtils::reorder_candidates_with_single_helpcode(base_candidates, help_code);
        }
    }

    return dictionary_.query(request.raw_input, request.segmentation);
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
