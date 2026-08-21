#include "romaji_converter.h"

#include <boost/locale/encoding_utf.hpp>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace
{
const std::unordered_map<std::string, std::string> &RomajiTable()
{
    static const std::unordered_map<std::string, std::string> table = {
        {"a", "あ"},   {"i", "い"},   {"u", "う"},   {"e", "え"},   {"o", "お"},
        {"ka", "か"},  {"ki", "き"},  {"ku", "く"},  {"ke", "け"},  {"ko", "こ"},
        {"ga", "が"},  {"gi", "ぎ"},  {"gu", "ぐ"},  {"ge", "げ"},  {"go", "ご"},
        {"sa", "さ"},  {"shi", "し"}, {"si", "し"},  {"su", "す"},  {"se", "せ"},
        {"so", "そ"},  {"za", "ざ"},  {"ji", "じ"},  {"zi", "じ"},  {"zu", "ず"},
        {"ze", "ぜ"},  {"zo", "ぞ"},  {"ta", "た"},  {"chi", "ち"}, {"ti", "ち"},
        {"tsu", "つ"}, {"tu", "つ"},  {"te", "て"},  {"to", "と"},  {"da", "だ"},
        {"di", "ぢ"},  {"du", "づ"},  {"de", "で"},  {"do", "ど"},  {"na", "な"},
        {"ni", "に"},  {"nu", "ぬ"},  {"ne", "ね"},  {"no", "の"},  {"ha", "は"},
        {"hi", "ひ"},  {"fu", "ふ"},  {"hu", "ふ"},  {"he", "へ"},  {"ho", "ほ"},
        {"ba", "ば"},  {"bi", "び"},  {"bu", "ぶ"},  {"be", "べ"},  {"bo", "ぼ"},
        {"pa", "ぱ"},  {"pi", "ぴ"},  {"pu", "ぷ"},  {"pe", "ぺ"},  {"po", "ぽ"},
        {"ma", "ま"},  {"mi", "み"},  {"mu", "む"},  {"me", "め"},  {"mo", "も"},
        {"ya", "や"},  {"yu", "ゆ"},  {"yo", "よ"},  {"ra", "ら"},  {"ri", "り"},
        {"ru", "る"},  {"re", "れ"},  {"ro", "ろ"},  {"wa", "わ"},  {"wo", "を"},
        {"nn", "ん"},  {"kya", "きゃ"}, {"kyu", "きゅ"}, {"kyo", "きょ"}, {"gya", "ぎゃ"},
        {"gyu", "ぎゅ"}, {"gyo", "ぎょ"}, {"sha", "しゃ"}, {"shu", "しゅ"}, {"sho", "しょ"},
        {"sya", "しゃ"}, {"syu", "しゅ"}, {"syo", "しょ"}, {"ja", "じゃ"},  {"ju", "じゅ"},
        {"jo", "じょ"},  {"jya", "じゃ"}, {"jyu", "じゅ"}, {"jyo", "じょ"}, {"cha", "ちゃ"},
        {"chu", "ちゅ"}, {"cho", "ちょ"}, {"cya", "ちゃ"}, {"cyu", "ちゅ"}, {"cyo", "ちょ"},
        {"tya", "ちゃ"}, {"tyu", "ちゅ"}, {"tyo", "ちょ"}, {"nya", "にゃ"}, {"nyu", "にゅ"},
        {"nyo", "にょ"}, {"hya", "ひゃ"}, {"hyu", "ひゅ"}, {"hyo", "ひょ"}, {"bya", "びゃ"},
        {"byu", "びゅ"}, {"byo", "びょ"}, {"pya", "ぴゃ"}, {"pyu", "ぴゅ"}, {"pyo", "ぴょ"},
        {"mya", "みゃ"}, {"myu", "みゅ"}, {"myo", "みょ"}, {"rya", "りゃ"}, {"ryu", "りゅ"},
        {"ryo", "りょ"}, {"fa", "ふぁ"},  {"fi", "ふぃ"},  {"fe", "ふぇ"},  {"fo", "ふぉ"},
        {"va", "ゔぁ"},  {"vi", "ゔぃ"},  {"vu", "ゔ"},   {"ve", "ゔぇ"},  {"vo", "ゔぉ"},
        {"wi", "うぃ"},  {"we", "うぇ"},  {"she", "しぇ"}, {"je", "じぇ"},  {"che", "ちぇ"},
        {"tsa", "つぁ"}, {"tsi", "つぃ"}, {"tse", "つぇ"}, {"tso", "つぉ"}, {"thi", "てぃ"},
        {"dhi", "でぃ"}, {"twu", "とぅ"}, {"dwu", "どぅ"}, {"kwa", "くぁ"}, {"gwa", "ぐぁ"},
        {"xa", "ぁ"},   {"xi", "ぃ"},   {"xu", "ぅ"},   {"xe", "ぇ"},   {"xo", "ぉ"},
        {"la", "ぁ"},   {"li", "ぃ"},   {"lu", "ぅ"},   {"le", "ぇ"},   {"lo", "ぉ"},
        {"xya", "ゃ"},  {"xyu", "ゅ"},  {"xyo", "ょ"},  {"lya", "ゃ"},  {"lyu", "ゅ"},
        {"lyo", "ょ"},  {"xtsu", "っ"}, {"ltsu", "っ"}, {"xwa", "ゎ"},  {"-", "ー"},
    };
    return table;
}

bool IsConsonant(char ch)
{
    return ch >= 'a' && ch <= 'z' && ch != 'a' && ch != 'i' && ch != 'u' && ch != 'e' && ch != 'o';
}
} // namespace

namespace japanese
{
RomajiConversion ConvertRomaji(std::string_view input)
{
    std::string normalized(input);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    RomajiConversion result;
    const auto &table = RomajiTable();
    size_t index = 0;
    while (index < normalized.size())
    {
        if (normalized[index] == 'n')
        {
            if (index + 1 == normalized.size())
            {
                result.hiragana += "ん";
                ++index;
                continue;
            }
            const char next = normalized[index + 1];
            if (next == '\'')
            {
                result.hiragana += "ん";
                index += 2;
                continue;
            }
            if (next == 'n' || (IsConsonant(next) && next != 'y'))
            {
                result.hiragana += "ん";
                ++index;
                continue;
            }
        }

        if (index + 1 < normalized.size() && normalized[index] == normalized[index + 1] &&
            IsConsonant(normalized[index]) && normalized[index] != 'n')
        {
            result.hiragana += "っ";
            ++index;
            continue;
        }

        bool matched = false;
        const size_t remaining = normalized.size() - index;
        for (size_t length = (std::min)(size_t{4}, remaining); length > 0; --length)
        {
            const auto found = table.find(normalized.substr(index, length));
            if (found == table.end())
            {
                continue;
            }
            result.hiragana += found->second;
            index += length;
            matched = true;
            break;
        }
        if (!matched)
        {
            result.pending = normalized.substr(index);
            break;
        }
    }
    result.complete = !result.hiragana.empty() && result.pending.empty();
    return result;
}

std::string HiraganaToKatakana(std::string_view hiragana)
{
    std::u32string codepoints = boost::locale::conv::utf_to_utf<char32_t>(hiragana.data(), hiragana.data() + hiragana.size());
    for (char32_t &codepoint : codepoints)
    {
        if (codepoint >= U'ぁ' && codepoint <= U'ゖ')
        {
            codepoint += 0x60;
        }
    }
    return boost::locale::conv::utf_to_utf<char>(codepoints);
}
} // namespace japanese
