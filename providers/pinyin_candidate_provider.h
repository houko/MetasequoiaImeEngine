#pragma once

#include "candidate_provider.h"
#include "../quanpin/engine.h"
#include "../shuangpin/engine.h"

class PinyinCandidateProvider : public ICandidateProvider
{
  public:
    explicit PinyinCandidateProvider(const ShuangpinProfile &shuangpin_profile = GetXiaoheShuangpinProfile());
    std::vector<WordItem> query(const QueryRequest &request) override;
    void reset_cache() override;
    int create_word(SchemeType scheme, std::string pinyin, std::string word) override;
    int update_weight_by_pinyin_and_word(SchemeType scheme, std::string pinyin, std::string word) override;
    int delete_by_pinyin_and_word(SchemeType scheme, std::string pinyin, std::string word) override;
    int cache_dynamic_candidate(SchemeType scheme, const std::string &pinyin, const std::string &word) override;
    int cache_dynamic_candidate_for_request(const QueryRequest &request, const std::string &word) override;

  private:
    const ShuangpinProfile &shuangpin_profile_;
    QuanpinEngine quanpin_engine_;
    ShuangpinEngine shuangpin_engine_;
};
