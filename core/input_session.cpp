#include "input_session.h"

#include "../common/helpcode_utils.h"
#include "../user_dictionary/user_dictionary_journal.h"
#include "data_path.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace metasequoia
{
namespace
{
const char *frequency_mode_name(FrequencyAdjustmentMode mode)
{
    switch (mode)
    {
    case FrequencyAdjustmentMode::Disabled:
        return "disabled";
    case FrequencyAdjustmentMode::Pin:
        return "pin";
    case FrequencyAdjustmentMode::Halve:
        return "halve";
    case FrequencyAdjustmentMode::Linear:
        return "linear";
    case FrequencyAdjustmentMode::Promote:
        return "promote";
    }
    return nullptr;
}
} // namespace

InputSession::InputSession(SchemeType scheme_type, bool quanpin_autocorrect_enabled, bool helpcode_enabled,
                           bool chinese_punctuation_enabled, bool candidate_learning_enabled)
    : engine_(scheme_type), quanpin_autocorrect_enabled_(quanpin_autocorrect_enabled),
      helpcode_enabled_(helpcode_enabled), chinese_punctuation_enabled_(chinese_punctuation_enabled),
      candidate_learning_enabled_(candidate_learning_enabled)
{
    engine_.set_quanpin_autocorrect_enabled(quanpin_autocorrect_enabled_);
    engine_.set_quanpin_helpcode_enabled(helpcode_enabled_);
    engine_.set_shuangpin_helpcode_enabled(helpcode_enabled_);
}

KeyResult InputSession::handle_character(char character)
{
    if ((character < 'a' || character > 'z') && character != '\'')
    {
        return {};
    }
    if (character == '\'' && !has_composition())
    {
        return {};
    }

    const std::string previous_preedit = preedit();
    const auto unsigned_character = static_cast<unsigned char>(character);
    const ImeKeyCode key_code =
        character == '\'' ? ImeKey::Apostrophe : static_cast<ImeKeyCode>(std::toupper(unsigned_character));
    engine_.handle_key(key_code, 0, static_cast<ImeCharacter>(unsigned_character));
    return {preedit() != previous_preedit, std::nullopt, std::nullopt};
}

KeyResult InputSession::handle_candidate_key(char character)
{
    if (!has_composition() || character < '1' || character > '9')
    {
        return {};
    }
    return select_candidate(static_cast<std::size_t>(character - '1'));
}

KeyResult InputSession::handle_punctuation(char character)
{
    if (!chinese_punctuation_enabled_)
    {
        return {};
    }

    const char *punctuation = nullptr;
    switch (character)
    {
    case ',':
        punctuation = "，";
        break;
    case '.':
        punctuation = "。";
        break;
    case '?':
        punctuation = "？";
        break;
    case '!':
        punctuation = "！";
        break;
    case ';':
        punctuation = "；";
        break;
    case ':':
        punctuation = "：";
        break;
    case '"':
        punctuation = next_double_quote_is_opening_ ? "“" : "”";
        next_double_quote_is_opening_ = !next_double_quote_is_opening_;
        break;
    case '\'':
        punctuation = next_single_quote_is_opening_ ? "‘" : "’";
        next_single_quote_is_opening_ = !next_single_quote_is_opening_;
        break;
    case '(':
        punctuation = "（";
        break;
    case ')':
        punctuation = "）";
        break;
    case '[':
        punctuation = "【";
        break;
    case ']':
        punctuation = "】";
        break;
    case '<':
        punctuation = "《";
        break;
    case '>':
        punctuation = "》";
        break;
    case '\\':
        punctuation = "、";
        break;
    default:
        return {};
    }

    std::string text;
    std::optional<std::string> diagnostic;
    if (has_composition())
    {
        if (candidates().empty())
        {
            text = preedit();
        }
        else
        {
            text = candidates().front().word;
            diagnostic = learn_candidate(0);
        }
        engine_.reset();
    }
    text += punctuation;
    return {true, std::move(text), std::move(diagnostic)};
}

KeyResult InputSession::handle_command(Command command)
{
    if (!has_composition())
    {
        return {};
    }

    switch (command)
    {
    case Command::Backspace:
        engine_.handle_key(ImeKey::Backspace);
        return {true, std::nullopt, std::nullopt};
    case Command::CommitCandidate:
        return commit(0);
    case Command::CommitRaw: {
        std::string raw = preedit();
        engine_.reset();
        return {true, std::move(raw), std::nullopt};
    }
    case Command::Cancel:
        engine_.reset();
        return {true, std::nullopt, std::nullopt};
    }
    return {};
}

KeyResult InputSession::select_candidate(std::size_t index)
{
    if (!has_composition() || index >= candidates().size())
    {
        return {};
    }
    return commit(index);
}

KeyResult InputSession::select_candidate(const std::string &candidate)
{
    const auto found = std::find_if(candidates().begin(), candidates().end(),
                                    [&](const WordItem &item) { return item.word == candidate; });
    if (found == candidates().end())
    {
        return {};
    }
    return commit(static_cast<std::size_t>(std::distance(candidates().begin(), found)));
}

KeyResult InputSession::select_candidate_edge(std::size_t index, CandidateEdge edge)
{
    if (!has_composition() || index >= candidates().size())
    {
        return {};
    }

    const std::string &candidate = candidates()[index].word;
    std::string character = edge == CandidateEdge::FirstHan ? HelpcodeUtils::get_first_han_char(candidate)
                                                             : HelpcodeUtils::get_last_han_char(candidate);
    if (character.empty())
    {
        return {};
    }

    engine_.reset();
    return {true, std::move(character), std::nullopt};
}

void InputSession::set_shuangpin_helpcode_enabled(bool enabled)
{
    engine_.set_shuangpin_helpcode_enabled(enabled);
}

void InputSession::set_quanpin_helpcode_enabled(bool enabled)
{
    engine_.set_quanpin_helpcode_enabled(enabled);
}

bool InputSession::is_supported_helpcode_schema(const std::string &schema)
{
    return HelpcodeUtils::is_supported_helpcode_schema(schema);
}

bool InputSession::select_helpcode_schema(const std::string &schema)
{
    return HelpcodeUtils::select_helpcode_schema(schema);
}

bool InputSession::set_frequency_adjustment(FrequencyAdjustmentOptions options)
{
    if (frequency_mode_name(options.mode) == nullptr || options.trigger_count < 1 || options.trigger_count > 10 ||
        options.linear_step < 1 || options.linear_step > 10)
    {
        return false;
    }
    frequency_adjustment_ = options;
    frequency_adjustment_configured_ = true;
    return true;
}

const FrequencyAdjustmentOptions &InputSession::frequency_adjustment() const
{
    return frequency_adjustment_;
}

void InputSession::switch_scheme(SchemeType scheme_type)
{
    engine_.switch_scheme(scheme_type);
}

SchemeType InputSession::scheme() const
{
    return engine_.current_scheme_type();
}

bool InputSession::has_composition() const
{
    return !preedit().empty();
}

const std::string &InputSession::preedit() const
{
    return engine_.get_preedit();
}

const std::string &InputSession::raw_segmentation() const
{
    return engine_.get_request().raw_segmentation;
}

const std::string &InputSession::normalized_segmentation() const
{
    return engine_.get_request().normalized_segmentation;
}

const std::vector<WordItem> &InputSession::candidates() const
{
    return engine_.get_candidates();
}

SchemeType InputSession::scheme_type() const
{
    return engine_.current_scheme_type();
}

bool InputSession::quanpin_autocorrect_enabled() const
{
    return quanpin_autocorrect_enabled_;
}

bool InputSession::helpcode_enabled() const
{
    return helpcode_enabled_;
}

bool InputSession::chinese_punctuation_enabled() const
{
    return chinese_punctuation_enabled_;
}

bool InputSession::candidate_learning_enabled() const
{
    return candidate_learning_enabled_;
}

KeyResult InputSession::commit(std::size_t index)
{
    std::string text = index < candidates().size() ? candidates()[index].word : preedit();
    std::optional<std::string> diagnostic = learn_candidate(index);
    engine_.reset();
    return {true, std::move(text), std::move(diagnostic)};
}

std::optional<std::string> InputSession::learn_candidate(std::size_t index)
{
    if (!candidate_learning_enabled_ || index >= candidates().size())
    {
        return std::nullopt;
    }

    const WordItem &selected = candidates()[index];
    if ((selected.source != CandidateSource::Database && selected.source != CandidateSource::UserDatabase) ||
        scheme() == SchemeType::JapaneseRomaji)
    {
        return std::nullopt;
    }

    if (!frequency_adjustment_configured_)
    {
        const std::string &pinyin =
            selected.canonical_pinyin.empty() ? selected.pinyin : selected.canonical_pinyin;
        (void)engine_.update_weight_by_pinyin_and_word(pinyin, selected.word);
        return std::nullopt;
    }
    if (frequency_adjustment_.mode == FrequencyAdjustmentMode::Disabled || index == 0)
    {
        return std::nullopt;
    }

    const bool wubi = scheme() == SchemeType::Wubi;
    std::string context_key = wubi ? engine_.get_request().raw_input : engine_.get_request().normalized_segmentation;
    if (context_key.empty())
    {
        context_key = engine_.get_request().segmentation;
    }
    const std::string entry_key = wubi ? selected.pinyin
                                       : (selected.canonical_pinyin.empty() ? context_key
                                                                          : selected.canonical_pinyin);
    bool ranking_changed = false;
    const bool adjusted = user_dictionary::adjust_candidate_ranking(
        path_to_utf8(data_file_path("msime.db")), user_dictionary::default_user_db_path(), context_key,
        candidates(), entry_key, selected.word, frequency_mode_name(frequency_adjustment_.mode),
        frequency_adjustment_.linear_step, frequency_adjustment_.trigger_count, false, &ranking_changed,
        wubi ? user_dictionary::DictionaryKind::Wubi : user_dictionary::DictionaryKind::Pinyin);
    if (!adjusted)
    {
        return std::string("Unable to persist candidate frequency adjustment.");
    }
    if (ranking_changed)
    {
        engine_.reset_cache();
    }
    return std::nullopt;
}
} // namespace metasequoia
