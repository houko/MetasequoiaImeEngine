#pragma once

#include "pinyin_candidate_provider.h"
#include "../core/scheme_type.h"
#include <string>

class ProviderRegistry
{
  public:
    ICandidateProvider &resolve(SchemeType scheme_type);
    void reset_cache(SchemeType scheme_type);
    int create_word(SchemeType scheme_type, std::string pinyin, std::string word);
    int update_weight_by_pinyin_and_word(SchemeType scheme_type, std::string pinyin, std::string word);
    int delete_by_pinyin_and_word(SchemeType scheme_type, std::string pinyin, std::string word);
    int cache_dynamic_candidate(SchemeType scheme_type, const std::string &pinyin, const std::string &word);
    int cache_dynamic_candidate_for_request(const QueryRequest &request, const std::string &word);

  private:
    PinyinCandidateProvider pinyin_provider_;
};
