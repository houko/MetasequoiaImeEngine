#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace japanese
{
struct SentenceCandidate
{
    std::string text;
    std::int64_t cost = 0;
};

class JapaneseSentenceDecoder
{
  public:
    explicit JapaneseSentenceDecoder(std::string model_path = {});
    bool ready() const { return ready_; }
    std::vector<SentenceCandidate> Decode(const std::string &reading, size_t limit = 8) const;

  private:
    struct Token
    {
        std::string reading;
        std::string surface;
        std::uint16_t left_id = 0;
        std::uint16_t right_id = 0;
        std::int32_t word_cost = 0;
    };

    bool Load(const std::string &path);
    int ConnectionCost(std::uint16_t right_id, std::uint16_t left_id) const;

    bool ready_ = false;
    std::uint32_t connection_size_ = 0;
    std::vector<Token> tokens_;
    std::vector<std::int16_t> connection_costs_;
    std::unordered_map<std::string, std::vector<std::uint32_t>> reading_index_;
};
} // namespace japanese
