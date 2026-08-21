#pragma once

#include <string>
#include <string_view>

namespace japanese
{
struct RomajiConversion
{
    std::string hiragana;
    std::string pending;
    bool complete = false;
};

RomajiConversion ConvertRomaji(std::string_view input);
std::string HiraganaToKatakana(std::string_view hiragana);
} // namespace japanese
