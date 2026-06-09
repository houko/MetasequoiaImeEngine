#pragma once

#include "candidate_provider.h"
#include "../quanpin/engine.h"
#include "../shuangpin/engine.h"

class PinyinCandidateProvider : public ICandidateProvider
{
  public:
    std::vector<WordItem> query(const QueryRequest &request) override;
    void reset_cache() override;

  private:
    QuanpinEngine quanpin_engine_;
    ShuangpinEngine shuangpin_engine_;
};
