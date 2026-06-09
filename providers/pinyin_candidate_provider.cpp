#include "pinyin_candidate_provider.h"
#include "../core/scheme_type.h"
#include <fmt/core.h>
#include <fmt/base.h>

std::vector<WordItem> PinyinCandidateProvider::query(const QueryRequest &request)
{
    if (!request.valid)
    {
        return {};
    }

    if (request.scheme == SchemeType::Shuangpin)
    {
        return shuangpin_engine_.query(request);
    }

    if (request.scheme == SchemeType::Quanpin)
    {
        return quanpin_engine_.query(request);
    }

    return {};
}

void PinyinCandidateProvider::reset_cache()
{
    quanpin_engine_.reset_cache();
    shuangpin_engine_.reset_cache();
}
