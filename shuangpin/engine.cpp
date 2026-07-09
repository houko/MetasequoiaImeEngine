#include "engine.h"

#include "shuangpin_query.h"
#include "shuangpin_utils.h"

namespace
{
std::vector<WordItem> query_normal(ShuangpinDictionary &dictionary, const QueryRequest &request)
{
    const std::string pure_input = shuangpin::remove_manual_delimiters(request.raw_input);
    return dictionary.generateSeries(pure_input, shuangpin::segment_input(request.raw_input));
}
} // namespace

std::vector<WordItem> ShuangpinEngine::query(const QueryRequest &request)
{
    if (!request.valid)
    {
        return {};
    }

    const std::string &raw_input = request.raw_input;
    const std::string &raw_input_with_cases =
        request.raw_input_with_cases.empty() ? request.raw_input : request.raw_input_with_cases;
    const std::string pure_input = shuangpin::remove_manual_delimiters(raw_input);
    const std::string pure_input_with_cases = shuangpin::remove_manual_delimiters(raw_input_with_cases);

    if (pure_input.empty())
    {
        return {};
    }

    if (request.enable_shuangpin_helpcode)
    {
        // 双码辅助
        if (ShuangpinUtil::IsFullHelpMode(pure_input_with_cases))
        {
            const std::string base_raw_input = pure_input.substr(0, pure_input.size() - 2);
            const std::string base_raw_segmentation = shuangpin::segment_input(base_raw_input);
            const std::string help_codes = pure_input.substr(pure_input.size() - 2, 2);
            return dictionary_.generate_with_helpcodes(base_raw_input, base_raw_segmentation, raw_input, help_codes);
        }

        // 单码辅助
        if (pure_input.size() % 2 == 1 && pure_input.size() > 1)
        {
            const std::string base_raw_input = pure_input.substr(0, pure_input.size() - 1);
            const std::string base_raw_segmentation = shuangpin::segment_input(base_raw_input);
            if (ShuangpinUtil::is_all_complete_pinyin(base_raw_input, base_raw_segmentation))
            {
                const std::string help_codes = pure_input.substr(pure_input.size() - 1, 1);
                return dictionary_.generate_with_helpcodes(base_raw_input, base_raw_segmentation, raw_input,
                                                           help_codes);
            }
        }

        // 不满足辅助码条件，单独查询，比如，cls -> c'ls，也就直接走下面的 query_normal 了
    }

    return query_normal(dictionary_, request);
}

int ShuangpinEngine::create_word(std::string pinyin, std::string word)
{
    return dictionary_.create_word(std::move(pinyin), std::move(word));
}

int ShuangpinEngine::update_weight_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinEngine::delete_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinEngine::insert_word_to_series_cache(const std::string &pinyin, const std::string &word)
{
    return dictionary_.insert_word_to_cached_buffer_series(pinyin, word);
}

int ShuangpinEngine::insert_word_to_active_helpcode_cache(const std::string &pinyin, const std::string &word)
{
    return dictionary_.insert_word_to_active_helpcode_cache(pinyin, word);
}

std::string ShuangpinEngine::search_sentence_from_ime_engine(const std::string &user_pinyin)
{
    return dictionary_.search_sentence_from_ime_engine(user_pinyin);
}

void ShuangpinEngine::reset_cache()
{
    dictionary_.reset_cache();
}
