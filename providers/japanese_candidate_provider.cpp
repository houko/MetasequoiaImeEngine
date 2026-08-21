#include "japanese_candidate_provider.h"
#include "../japanese/romaji_converter.h"
#include "../quanpin/quanpin_query.h"
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace
{
constexpr int kNoMutation = 0;

void AppendUnique(std::vector<WordItem> &items, std::unordered_set<std::string> &seen,
                  const std::string &code, const std::string &value, std::int64_t weight,
                  CandidateSource source = CandidateSource::Database)
{
    if (!value.empty() && seen.insert(value).second)
    {
        items.emplace_back(code, value, weight, source, code);
    }
}
} // namespace

JapaneseCandidateProvider::JapaneseCandidateProvider(std::string db_path)
    : db_path_(db_path.empty() ? quanpin::get_default_db_path() : std::move(db_path))
{
}

JapaneseCandidateProvider::~JapaneseCandidateProvider()
{
    close_database();
}

std::vector<WordItem> JapaneseCandidateProvider::query(const QueryRequest &request)
{
    if (!request.valid || request.scheme != SchemeType::JapaneseRomaji)
    {
        return {};
    }

    std::vector<WordItem> candidates;
    std::unordered_set<std::string> seen;
    const auto conversion = japanese::ConvertRomaji(request.raw_input);
    if (conversion.complete)
    {
        if (!sentence_decoder_)
            sentence_decoder_ = std::make_unique<japanese::JapaneseSentenceDecoder>();
        for (const auto &sentence : sentence_decoder_->Decode(conversion.hiragana, 8))
        {
            AppendUnique(candidates, seen, request.raw_input_with_cases, sentence.text,
                         2000000 - sentence.cost, CandidateSource::Database);
        }
        AppendUnique(candidates, seen, request.raw_input_with_cases, conversion.hiragana, 1000000,
                     CandidateSource::Generated);
        AppendUnique(candidates, seen, request.raw_input_with_cases,
                     japanese::HiraganaToKatakana(conversion.hiragana), 999999, CandidateSource::Generated);
    }

    if (!ensure_query_statement())
    {
        return candidates;
    }

    sqlite3_reset(query_statement_);
    sqlite3_clear_bindings(query_statement_);
    sqlite3_bind_text(query_statement_, 1, request.raw_input_with_cases.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(query_statement_, 2, request.raw_input.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(query_statement_) == SQLITE_ROW)
    {
        const auto *code = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 0));
        const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 1));
        if (code && value)
        {
            AppendUnique(candidates, seen, code, value, sqlite3_column_int64(query_statement_, 2));
        }
    }
    return candidates;
}

std::optional<WordItem> JapaneseCandidateProvider::find_candidate(
    SchemeType scheme, const std::string &key, const std::string &value)
{
    if (scheme != SchemeType::JapaneseRomaji || !ensure_query_statement()) return std::nullopt;
    sqlite3_reset(query_statement_);
    sqlite3_clear_bindings(query_statement_);
    sqlite3_bind_text(query_statement_, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(query_statement_, 2, key.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(query_statement_) == SQLITE_ROW)
    {
        const auto *code = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 0));
        const auto *candidate = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 1));
        if (code && candidate && value == candidate)
            return WordItem(code, candidate, sqlite3_column_int64(query_statement_, 2), CandidateSource::Database, code);
    }
    return std::nullopt;
}

void JapaneseCandidateProvider::reset_cache()
{
    close_database();
    sentence_decoder_.reset();
}

int JapaneseCandidateProvider::create_word(SchemeType, std::string, std::string) { return kNoMutation; }
int JapaneseCandidateProvider::update_weight_by_pinyin_and_word(SchemeType, std::string, std::string) { return kNoMutation; }
int JapaneseCandidateProvider::delete_by_pinyin_and_word(SchemeType, std::string, std::string) { return kNoMutation; }
int JapaneseCandidateProvider::cache_dynamic_candidate(SchemeType, const std::string &, const std::string &,
                                                       CandidateSource) { return kNoMutation; }
int JapaneseCandidateProvider::cache_dynamic_candidate_for_request(const QueryRequest &, const std::string &,
                                                                   CandidateSource) { return kNoMutation; }

bool JapaneseCandidateProvider::ensure_query_statement()
{
    if (query_statement_) return true;
    if (!db_ && sqlite3_open_v2(db_path_.c_str(), &db_, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
    {
        close_database();
        return false;
    }
    constexpr const char *sql =
        "SELECT code, value, weight FROM japanese_lexicon "
        "WHERE code=?1 OR code=?2 ORDER BY weight DESC, rowid ASC";
    if (sqlite3_prepare_v2(db_, sql, -1, &query_statement_, nullptr) != SQLITE_OK)
    {
        close_database();
        return false;
    }
    return true;
}

void JapaneseCandidateProvider::close_database()
{
    if (query_statement_)
    {
        sqlite3_finalize(query_statement_);
        query_statement_ = nullptr;
    }
    if (db_)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}
