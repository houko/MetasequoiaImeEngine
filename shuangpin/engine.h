#pragma once

#include "../core/query_request.h"
#include "../core/word_item.h"
#include "shuangpin_dictionary.h"
#include <string>
#include <vector>

class ShuangpinEngine
{
  public:
    explicit ShuangpinEngine(const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
    std::vector<WordItem> query(const QueryRequest &request);
    int create_word(std::string pinyin, std::string word);
    int update_weight_by_pinyin_and_word(std::string pinyin, std::string word);
    int delete_by_pinyin_and_word(std::string pinyin, std::string word);
    int insert_word_to_series_cache(const std::string &pinyin, const std::string &word);
    int insert_word_to_active_helpcode_cache(const std::string &pinyin, const std::string &word);
    std::string search_sentence_from_ime_engine(const std::string &user_pinyin);
    void reset_cache();

  private:
    const ShuangpinProfile &profile_;
    ShuangpinDictionary dictionary_;
};
