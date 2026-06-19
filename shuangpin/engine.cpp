#include "engine.h"

#include "shuangpin_query.h"

std::vector<WordItem> ShuangpinEngine::query(const QueryRequest &request)
{
    if (!request.valid)
    {
        return {};
    }
    return dictionary_.generateSeries(request.raw_input, shuangpin::segment_input(request.raw_input));
}

std::string ShuangpinEngine::search_sentence_from_ime_engine(const std::string &user_pinyin)
{
    return dictionary_.search_sentence_from_ime_engine(user_pinyin);
}

void ShuangpinEngine::reset_cache()
{
    dictionary_.reset_cache();
}
