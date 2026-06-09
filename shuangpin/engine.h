#pragma once

#include "../core/query_request.h"
#include "../core/word_item.h"
#include "shuangpin_dictionary.h"
#include <string>
#include <vector>

class ShuangpinEngine
{
  public:
    std::vector<WordItem> query(const QueryRequest &request);
    std::string search_sentence_from_ime_engine(const std::string &user_pinyin);
    void reset_cache();

  private:
    ShuangpinDictionary dictionary_;
};
