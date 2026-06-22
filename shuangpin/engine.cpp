#include "engine.h"

#include "shuangpin_query.h"
#include "shuangpin_utils.h"

namespace
{
std::vector<WordItem> query_normal(ShuangpinDictionary &dictionary, const QueryRequest &request)
{
    return dictionary.generateSeries(request.raw_input, shuangpin::segment_input(request.raw_input));
}
}

std::vector<WordItem> ShuangpinEngine::query(const QueryRequest &request)
{
    if (!request.valid)
    {
        return {};
    }

    const std::string &raw_input = request.raw_input;
    const std::string &raw_input_with_cases =
        request.raw_input_with_cases.empty() ? request.raw_input : request.raw_input_with_cases;

    if (raw_input.empty())
    {
        return {};
    }

    if (request.enable_shuangpin_helpcode)
    {
        if (ShuangpinUtil::IsFullHelpMode(raw_input_with_cases))
        {
            const std::string base_raw_input = raw_input.substr(0, raw_input.size() - 2);
            const std::string base_raw_segmentation = shuangpin::segment_input(base_raw_input);
            const std::string help_codes = raw_input.substr(raw_input.size() - 2, 2);
            return dictionary_.generate_with_helpcodes(base_raw_input, base_raw_segmentation, raw_input, help_codes);
        }

        if (raw_input.size() % 2 == 1 && raw_input.size() > 1)
        {
            const std::string base_raw_input = raw_input.substr(0, raw_input.size() - 1);
            const std::string base_raw_segmentation = shuangpin::segment_input(base_raw_input);
            if (ShuangpinUtil::is_all_complete_pinyin(base_raw_input, base_raw_segmentation))
            {
                const std::string help_codes = raw_input.substr(raw_input.size() - 1, 1);
                return dictionary_.generate_with_helpcodes(base_raw_input, base_raw_segmentation, raw_input, help_codes);
            }
        }
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
