#include "helpcode_utils.h"

#include <utf8.h>
#include <algorithm>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <spdlog/spdlog.h>

namespace
{
std::string get_local_appdata_path()
{
    char *localAppDataDir = nullptr;
    std::string localAppDataDirStr;

    errno_t err = _dupenv_s(&localAppDataDir, nullptr, "LOCALAPPDATA");
    if (err == 0 && localAppDataDir != nullptr)
    {
        localAppDataDirStr = std::string(localAppDataDir);
    }

    std::unique_ptr<char, decltype(&free)> dirPtr(localAppDataDir, free);
    return localAppDataDirStr.empty() ? "" : localAppDataDirStr;
}

const std::string kAppName = "metasequoiaime";
const std::string kPathSeparator = "\\";
const std::string kLantianHelpcodeFileName = "helpcode.txt";
const std::string kZiranmaHelpcodeFileName = "zrm_helpcode_big_unique.txt";
std::atomic_bool g_use_ziranma{false};

std::unordered_map<std::string, std::string> initialize_helpcode_keymap(const std::string &file_name)
{
    std::unordered_map<std::string, std::string> result;
    std::ifstream helpcode_path(get_local_appdata_path() + kPathSeparator + kAppName + kPathSeparator + file_name);
    if (!helpcode_path.is_open())
    {
        (void)0;
        return result;
    }

    std::string line;
    while (getline(helpcode_path, line))
    {
        size_t pos = line.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }
        result[line.substr(0, pos)] = line.substr(pos + 1, 2);
    }
    return result;
}
} // namespace

namespace HelpcodeUtils
{

const std::unordered_map<std::string, std::string> &helpcode_keymap()
{
    static const auto lantian_keymap = initialize_helpcode_keymap(kLantianHelpcodeFileName);
    static const auto ziranma_keymap = initialize_helpcode_keymap(kZiranmaHelpcodeFileName);
    return g_use_ziranma.load(std::memory_order_relaxed) ? ziranma_keymap : lantian_keymap;
}

bool select_helpcode_schema(const std::string &schema)
{
    if (schema != "lantian" && schema != "ziranma")
        return false;
    g_use_ziranma.store(schema == "ziranma", std::memory_order_relaxed);
    return true;
}

std::string get_first_han_char(const std::string &words)
{
    auto it = words.begin();
    auto end = words.end();

    if (it == end)
    {
        return "";
    }

    auto next = it;
    utf8::next(next, end);
    return std::string(it, next);
}

namespace
{
std::string::size_type get_first_char_size(const std::string &words)
{
    size_t cplen = 1;
    if ((words[0] & 0xf8) == 0xf0)
    {
        cplen = 4;
    }
    else if ((words[0] & 0xf0) == 0xe0)
    {
        cplen = 3;
    }
    else if ((words[0] & 0xe0) == 0xc0)
    {
        cplen = 2;
    }
    if (cplen > words.length())
    {
        cplen = 1;
    }
    return cplen;
}
} // namespace

std::string get_last_han_char(const std::string &words)
{
    auto it = words.begin();
    auto end = words.end();

    if (it == end)
    {
        return "";
    }

    auto rit = words.end();
    auto prev = rit;
    utf8::prior(prev, it);
    return std::string(prev, rit);
}

std::string::size_type count_han_chars(const std::string &words)
{
    size_t index = 0;
    size_t cnt = 0;
    while (index < words.size())
    {
        size_t cplen = get_first_char_size(words.substr(index, words.size() - index));
        index += cplen;
        cnt += 1;
    }
    return cnt;
}

std::string::size_type count_utf8_chars(const std::string &text)
{
    return utf8::distance(text.begin(), text.end());
}

std::string compute_helpcodes(const std::string &words, bool uppercase_all)
{
    std::string helpcodes;
    const auto &keymap = helpcode_keymap();

    if (count_han_chars(words) == 1)
    {
        const auto found = keymap.find(words);
        if (found != keymap.end())
        {
            helpcodes += found->second;
        }
    }
    else
    {
        const std::string firstHan = get_first_han_char(words);
        const auto first = keymap.find(firstHan);
        if (first != keymap.end())
        {
            helpcodes += first->second.substr(0, 1);
        }
        else
        {
            return "";
        }

        const std::string lastHan = get_last_han_char(words);
        const auto last = keymap.find(lastHan);
        if (last != keymap.end())
        {
            helpcodes += last->second.substr(0, 1);
        }
        else
        {
            return "";
        }
    }

    if (!helpcodes.empty())
    {
        if (uppercase_all)
        {
            std::transform(helpcodes.begin(), helpcodes.end(), helpcodes.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
        }
        else if (helpcodes.size() >= 2)
        {
            helpcodes[1] = static_cast<char>(toupper(static_cast<unsigned char>(helpcodes[1])));
        }
        helpcodes = "(" + helpcodes + ")";
    }
    return helpcodes;
}

bool is_quanpin_single_help_mode(const std::string &pinyin_with_cases)
{
    if (pinyin_with_cases.size() <= 1)
    {
        return false;
    }

    if (is_quanpin_double_help_mode(pinyin_with_cases))
    {
        return false;
    }

    const char help_code = pinyin_with_cases.back();
    return help_code >= 'A' && help_code <= 'Z';
}

bool is_quanpin_double_help_mode(const std::string &pinyin_with_cases)
{
    if (pinyin_with_cases.size() <= 2)
    {
        return false;
    }

    const char help_code_1 = pinyin_with_cases[pinyin_with_cases.size() - 2];
    const char help_code_2 = pinyin_with_cases[pinyin_with_cases.size() - 1];
    return help_code_1 >= 'A' && help_code_1 <= 'Z' && help_code_2 >= 'A' && help_code_2 <= 'Z';
}

} // namespace HelpcodeUtils
