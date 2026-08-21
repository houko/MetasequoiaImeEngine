#include "japanese_sentence_decoder.h"
#include "../shuangpin/shuangpin_utils.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace
{
#pragma pack(push, 1)
struct ModelHeader
{
    char magic[8];
    std::uint32_t version;
    std::uint32_t token_count;
    std::uint32_t connection_size;
    std::uint32_t reserved;
    std::uint64_t token_offset;
    std::uint64_t connection_offset;
    std::uint64_t string_offset;
    std::uint64_t string_size;
};

struct ModelToken
{
    std::uint32_t reading_offset;
    std::uint16_t reading_length;
    std::uint32_t surface_offset;
    std::uint16_t surface_length;
    std::uint16_t left_id;
    std::uint16_t right_id;
    std::int32_t word_cost;
};
#pragma pack(pop)

constexpr char kMagic[8] = {'M', 'S', 'J', 'P', 'D', 'T', '1', '\0'};
constexpr std::int64_t kUnknownKanaCost = 12000;

std::vector<size_t> Utf8Boundaries(const std::string &text)
{
    std::vector<size_t> boundaries{0};
    size_t index = 0;
    while (index < text.size())
    {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : 4;
        index = (std::min)(text.size(), index + length);
        boundaries.push_back(index);
    }
    return boundaries;
}
} // namespace

namespace japanese
{
JapaneseSentenceDecoder::JapaneseSentenceDecoder(std::string model_path)
{
    if (model_path.empty())
    {
        model_path = shuangpin::get_local_appdata_path() + "\\" + shuangpin::get_app_name() +
                     "\\dict_japanese.dat";
    }
    ready_ = Load(model_path);
}

bool JapaneseSentenceDecoder::Load(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    ModelHeader header{};
    if (!stream.read(reinterpret_cast<char *>(&header), sizeof(header)) ||
        std::memcmp(header.magic, kMagic, sizeof(kMagic)) != 0 || header.version != 1 ||
        header.connection_size == 0 || header.token_count > 2000000 || header.string_size > (1ull << 32))
        return false;

    std::vector<ModelToken> records(header.token_count);
    stream.seekg(static_cast<std::streamoff>(header.token_offset));
    if (!stream.read(reinterpret_cast<char *>(records.data()),
                     static_cast<std::streamsize>(records.size() * sizeof(ModelToken))))
        return false;

    const std::uint64_t connection_count =
        static_cast<std::uint64_t>(header.connection_size) * header.connection_size;
    if (connection_count > 20000000) return false;
    connection_costs_.resize(static_cast<size_t>(connection_count));
    stream.seekg(static_cast<std::streamoff>(header.connection_offset));
    if (!stream.read(reinterpret_cast<char *>(connection_costs_.data()),
                     static_cast<std::streamsize>(connection_costs_.size() * sizeof(std::int16_t))))
        return false;

    std::string strings(static_cast<size_t>(header.string_size), '\0');
    stream.seekg(static_cast<std::streamoff>(header.string_offset));
    if (!stream.read(strings.data(), static_cast<std::streamsize>(strings.size()))) return false;

    connection_size_ = header.connection_size;
    tokens_.reserve(records.size());
    for (const auto &record : records)
    {
        if (static_cast<std::uint64_t>(record.reading_offset) + record.reading_length > strings.size() ||
            static_cast<std::uint64_t>(record.surface_offset) + record.surface_length > strings.size() ||
            record.left_id >= connection_size_ || record.right_id >= connection_size_)
            return false;
        Token token;
        token.reading.assign(strings.data() + record.reading_offset, record.reading_length);
        token.surface.assign(strings.data() + record.surface_offset, record.surface_length);
        token.left_id = record.left_id;
        token.right_id = record.right_id;
        token.word_cost = record.word_cost;
        const auto index = static_cast<std::uint32_t>(tokens_.size());
        tokens_.push_back(std::move(token));
        reading_index_[tokens_.back().reading].push_back(index);
    }

    tokens_by_reading_.resize(tokens_.size());
    tokens_by_surface_.resize(tokens_.size());
    for (std::uint32_t i = 0; i < tokens_.size(); ++i)
    {
        tokens_by_reading_[i] = i;
        tokens_by_surface_[i] = i;
    }
    std::sort(tokens_by_reading_.begin(), tokens_by_reading_.end(),
              [this](std::uint32_t a, std::uint32_t b) { return tokens_[a].reading < tokens_[b].reading; });
    std::sort(tokens_by_surface_.begin(), tokens_by_surface_.end(),
              [this](std::uint32_t a, std::uint32_t b) { return tokens_[a].surface < tokens_[b].surface; });
    return !tokens_.empty();
}

int JapaneseSentenceDecoder::ConnectionCost(std::uint16_t right_id, std::uint16_t left_id) const
{
    if (right_id >= connection_size_ || left_id >= connection_size_) return 10000;
    return connection_costs_[static_cast<size_t>(right_id) * connection_size_ + left_id];
}

JapaneseLemma JapaneseSentenceDecoder::MakeLemma(std::uint32_t token_id) const
{
    const Token &token = tokens_[token_id];
    return {token.reading, token.surface, token.left_id, token.right_id, token.word_cost, token_id};
}

std::vector<JapaneseLemma> JapaneseSentenceDecoder::ExactLemmas(const std::string &reading, size_t limit) const
{
    std::vector<JapaneseLemma> result;
    if (!ready_ || reading.empty() || limit == 0) return result;
    const auto found = reading_index_.find(reading);
    if (found == reading_index_.end()) return result;
    for (const std::uint32_t token_id : found->second)
    {
        result.push_back(MakeLemma(token_id));
        if (result.size() == limit) break;
    }
    std::sort(result.begin(), result.end(),
              [](const JapaneseLemma &a, const JapaneseLemma &b) { return a.word_cost < b.word_cost; });
    return result;
}

std::vector<JapaneseLemma> JapaneseSentenceDecoder::PrefixLemmas(const std::string &reading_prefix,
                                                                 size_t limit) const
{
    std::vector<JapaneseLemma> result;
    if (!ready_ || reading_prefix.empty() || limit == 0) return result;
    auto it = std::lower_bound(tokens_by_reading_.begin(), tokens_by_reading_.end(), reading_prefix,
                               [this](std::uint32_t id, const std::string &prefix) {
                                   return tokens_[id].reading < prefix;
                               });
    for (; it != tokens_by_reading_.end(); ++it)
    {
        const Token &token = tokens_[*it];
        if (token.reading.size() < reading_prefix.size() ||
            token.reading.compare(0, reading_prefix.size(), reading_prefix) != 0)
            break;
        result.push_back(MakeLemma(*it));
    }
    std::sort(result.begin(), result.end(),
              [](const JapaneseLemma &a, const JapaneseLemma &b) { return a.word_cost < b.word_cost; });
    if (result.size() > limit) result.resize(limit);
    return result;
}

std::vector<JapaneseLemma> JapaneseSentenceDecoder::PrefixLemmasContinuing(
    const std::string &reading_prefix, const std::vector<std::string> &next_kana, size_t limit) const
{
    std::vector<JapaneseLemma> result;
    if (!ready_ || reading_prefix.empty() || next_kana.empty() || limit == 0) return result;
    auto it = std::lower_bound(tokens_by_reading_.begin(), tokens_by_reading_.end(), reading_prefix,
                               [this](std::uint32_t id, const std::string &prefix) {
                                   return tokens_[id].reading < prefix;
                               });
    for (; it != tokens_by_reading_.end(); ++it)
    {
        const Token &token = tokens_[*it];
        if (token.reading.size() <= reading_prefix.size() ||
            token.reading.compare(0, reading_prefix.size(), reading_prefix) != 0)
            break;
        const std::string_view remaining(token.reading.data() + reading_prefix.size(),
                                         token.reading.size() - reading_prefix.size());
        bool matched = false;
        for (const auto &kana : next_kana)
        {
            if (remaining.size() >= kana.size() && remaining.compare(0, kana.size(), kana) == 0)
            {
                matched = true;
                break;
            }
        }
        if (matched) result.push_back(MakeLemma(*it));
    }
    std::sort(result.begin(), result.end(),
              [](const JapaneseLemma &a, const JapaneseLemma &b) { return a.word_cost < b.word_cost; });
    if (result.size() > limit) result.resize(limit);
    return result;
}

std::vector<SentenceCandidate> JapaneseSentenceDecoder::Predict(const std::string &history_surface,
                                                                size_t limit) const
{
    std::vector<SentenceCandidate> result;
    if (!ready_ || history_surface.empty() || limit == 0) return result;
    const auto bounds = Utf8Boundaries(history_surface);
    if (bounds.size() < 2) return result;

    std::unordered_set<std::string> seen;
    const size_t char_count = bounds.size() - 1;
    const size_t max_hist = (std::min)(char_count, size_t{7});
    for (size_t hist_len = max_hist; hist_len > 0 && result.size() < limit; --hist_len)
    {
        const size_t start = bounds[char_count - hist_len];
        const std::string prefix = history_surface.substr(start);
        auto it = std::lower_bound(tokens_by_surface_.begin(), tokens_by_surface_.end(), prefix,
                                   [this](std::uint32_t id, const std::string &value) {
                                       return tokens_[id].surface < value;
                                   });
        for (; it != tokens_by_surface_.end() && result.size() < limit; ++it)
        {
            const Token &token = tokens_[*it];
            if (token.surface.size() <= prefix.size() ||
                token.surface.compare(0, prefix.size(), prefix) != 0)
                break;
            std::string rest = token.surface.substr(prefix.size());
            if (seen.insert(rest).second)
                result.push_back({std::move(rest), token.word_cost});
        }
    }
    std::sort(result.begin(), result.end(),
              [](const SentenceCandidate &a, const SentenceCandidate &b) { return a.cost < b.cost; });
    if (result.size() > limit) result.resize(limit);
    return result;
}

std::vector<SentenceCandidate> JapaneseSentenceDecoder::Decode(const std::string &reading, size_t limit) const
{
    if (!ready_ || reading.empty() || limit == 0) return {};
    struct Path { std::string text; std::int64_t cost; std::uint16_t right_id; };
    const auto boundaries = Utf8Boundaries(reading);
    std::vector<std::vector<Path>> paths(reading.size() + 1);
    paths[0].push_back({{}, 0, 0});
    const size_t beam = (std::max)(size_t{16}, limit * 4);

    for (size_t boundary_index = 0; boundary_index + 1 < boundaries.size(); ++boundary_index)
    {
        const size_t start = boundaries[boundary_index];
        if (paths[start].empty()) continue;
        for (size_t end_index = boundary_index + 1; end_index < boundaries.size(); ++end_index)
        {
            const size_t end = boundaries[end_index];
            const std::string key = reading.substr(start, end - start);
            const auto found = reading_index_.find(key);
            if (found == reading_index_.end()) continue;
            for (const std::uint32_t token_index : found->second)
            {
                const Token &token = tokens_[token_index];
                for (const auto &previous : paths[start])
                {
                    paths[end].push_back({previous.text + token.surface,
                                          previous.cost + token.word_cost +
                                              ConnectionCost(previous.right_id, token.left_id),
                                          token.right_id});
                }
            }
        }

        const size_t next = boundaries[boundary_index + 1];
        const std::string kana = reading.substr(start, next - start);
        for (const auto &previous : paths[start])
            paths[next].push_back({previous.text + kana, previous.cost + kUnknownKanaCost, 0});

        for (size_t end_index = boundary_index + 1; end_index < boundaries.size(); ++end_index)
        {
            auto &bucket = paths[boundaries[end_index]];
            if (bucket.size() > beam)
            {
                std::partial_sort(bucket.begin(), bucket.begin() + beam, bucket.end(),
                                  [](const Path &a, const Path &b) { return a.cost < b.cost; });
                bucket.resize(beam);
            }
        }
    }

    auto finals = std::move(paths[reading.size()]);
    for (auto &path : finals) path.cost += ConnectionCost(path.right_id, 0);
    std::sort(finals.begin(), finals.end(), [](const Path &a, const Path &b) { return a.cost < b.cost; });
    std::vector<SentenceCandidate> result;
    std::unordered_set<std::string> seen;
    for (auto &path : finals)
    {
        if (seen.insert(path.text).second) result.push_back({std::move(path.text), path.cost});
        if (result.size() == limit) break;
    }
    return result;
}
} // namespace japanese
