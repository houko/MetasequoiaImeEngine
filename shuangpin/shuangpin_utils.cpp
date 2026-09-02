#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utf8.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include "shuangpin_utils.h"
#include "../quanpin/quanpin_utils.h"
#include <windows.h>
#include <shlobj.h>

using namespace std;

const string ShuangpinUtil::app_name = "metasequoiaime";
static string path_seperator = "\\";

namespace
{
bool PathHasEmptyComponent(const std::wstring &path)
{
    if (path.empty())
    {
        return true;
    }
    size_t index = 0;
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
    {
        index = 2;
    }
    else if (path[0] == L'\\')
    {
        return true;
    }
    for (; index + 1 < path.size(); ++index)
    {
        if (path[index] == L'\\' && path[index + 1] == L'\\')
        {
            return true;
        }
    }
    return false;
}

bool IsUsableAbsolutePath(const std::wstring &path)
{
    if (PathHasEmptyComponent(path))
    {
        return false;
    }
    if (path.size() >= 2 && path[1] == L':')
    {
        return true;
    }
    return path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\';
}

std::wstring QueryLocalAppDataW()
{
    const DWORD needed = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (needed != 0)
    {
        std::wstring value(needed, L'\0');
        const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), needed);
        if (written != 0 && written < needed)
        {
            value.resize(written);
            if (IsUsableAbsolutePath(value))
            {
                return value;
            }
        }
    }

    PWSTR known_path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &known_path)) &&
        known_path)
    {
        std::wstring result(known_path);
        CoTaskMemFree(known_path);
        if (IsUsableAbsolutePath(result))
        {
            return result;
        }
    }
    return {};
}

std::string WideToUtf8(const std::wstring &wide)
{
    if (wide.empty())
    {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
    {
        return {};
    }
    std::string utf8(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), size, nullptr, nullptr);
    return utf8;
}
} // namespace

string ShuangpinUtil::get_local_appdata_path()
{
    return WideToUtf8(QueryLocalAppDataW());
}

// LocalAppData path
string ShuangpinUtil::local_appdata_path = ShuangpinUtil::get_local_appdata_path();

namespace
{
std::string find_source_by_code(const std::unordered_map<std::string, std::string> &mapping, const std::string &code)
{
    for (const auto &[source, mapped_code] : mapping)
    {
        if (mapped_code == code)
        {
            return source;
        }
    }
    return {};
}

const std::unordered_set<std::string> &shuangpin_pinyin_set()
{
    static const std::unordered_set<std::string> pinyin_set = [] {
        auto result = quanpin::intact_pinyin_set();

        // Keep this compiled fallback identical to the historical pinyin.txt used
        // for shuangpin validation.  The full quanpin table accepts a few extra
        // spellings whose presence changes ambiguous shuangpin segmentation.
        result.insert("eng");
        for (const char *pinyin : {"chua", "den", "fiao", "jve", "lo", "lue", "nou", "nue", "nun",
                                   "qve", "xve", "yo", "yve", "zhei"})
        {
            result.erase(pinyin);
        }
        return result;
    }();
    return pinyin_set;
}
} // namespace

/**
 * @brief Convert one shuangpin syllable to quanpin using the selected profile
 *
 * Currently support 402 pinyin
 *
 * @param sp_str Shuangpin string
 * @return string Quanpin string
 */
string ShuangpinUtil::cvt_single_sp_to_pinyin(string sp_str, const ShuangpinProfile &profile)
{
    const std::string zero_initial = find_source_by_code(profile.zero_initials, sp_str);
    if (!zero_initial.empty())
    {
        return zero_initial;
    }
    if (sp_str.size() != 2)
        return "";
    string res = "";
    string sm;
    vector<string> ym_list;

    sm = find_source_by_code(profile.initials, sp_str.substr(0, 1));
    if (sm.empty())
        sm = sp_str.substr(0, 1);

    for (const auto &pair : profile.finals)
    {
        if (pair.second == sp_str.substr(1, 1))
        {
            ym_list.push_back(pair.first);
        }
    }
    if (sm == "" || ym_list.size() == 0)
    {
        return "";
    }
    for (const auto &ym : ym_list)
    {
        std::string normalized_ym = ym;
        if (ym == "v" && (sm == "j" || sm == "q" || sm == "x" || sm == "y"))
        {
            normalized_ym = "u";
        }
        if (shuangpin_pinyin_set().count(sm + normalized_ym) > 0)
        {
            res = sm + normalized_ym;
        }
    }
    return res;
}

/**
 * @brief Split shuangpin, using ' as delimiter, using forward greedy segmentation
 *
 * @param sp_str Shuangpin string
 * @return string Segmented string with '
 */
string ShuangpinUtil::pinyin_segmentation(string sp_str, const ShuangpinProfile &profile)
{
    if (sp_str.size() == 1)
    {
        return sp_str;
    }
    string res("");
    string::size_type range_start = 0;
    while (range_start < sp_str.size())
    {
        if ((range_start + 2) <= sp_str.size())
        {
            // Try to cut two chars to test
            string cur_sp = sp_str.substr(range_start, 2);
            if (shuangpin_pinyin_set().count(
                    cvt_single_sp_to_pinyin(boost::algorithm::to_lower_copy(cur_sp), profile)) > 0)
            {
                res = res + "'" + cur_sp;
                range_start += 2;
            }
            else
            {
                res = res + "'" + cur_sp.substr(0, 1);
                range_start += 1;
            }
        }
        else
        {
            res = res + "'" + sp_str.substr(sp_str.size() - 1, 1);
            range_start += 1;
        }
    }
    while (!res.empty() && res[0] == '\'')
    {
        res.erase(0, 1);
    }
    while (!res.empty() && res[res.size()] == '\'')
    {
        res.erase(res.size() - 1, 1);
    }
    return res;
}

/**
 * @brief Get the first han char
 *
 * @param words
 * @return std::string
 */
/**
 * @brief Get first UTF-8 char size
 *
 * @param words UTF-8 string
 * @return string::size_type Char size
 */
string::size_type ShuangpinUtil::get_first_char_size(string words)
{
    size_t cplen = 1;
    if ((words[0] & 0xf8) == 0xf0)
        cplen = 4;
    else if ((words[0] & 0xf0) == 0xe0)
        cplen = 3;
    else if ((words[0] & 0xe0) == 0xc0)
        cplen = 2;
    if (cplen > words.length())
        cplen = 1;
    return cplen;
}

/**
 * @brief Count UTF-8 chars
 *
 * @param str
 * @return string::size_type
 */
string::size_type ShuangpinUtil::count_utf8_chars(const string &str)
{
    return utf8::distance(str.begin(), str.end());
}

/**
 * @brief Extract preview without helpcodes
 *
 * @param candidate UTF-8 string
 * @return string Pure hanzi string
 */
string ShuangpinUtil::extract_preview(string candidate)
{
    size_t start_pos = candidate.find('(');
    if (start_pos != string::npos)
    {
        return candidate.substr(0, start_pos);
    }
    return candidate;
}

/**
 * @brief Check if all pinyin is quanpin
 *
 * @param pure_pinyin Pure shuangpin
 * @param seg_pinyin Segmented shuangpin
 * @return true If all pinyin is quanpin
 * @return false Otherwise
 */
bool ShuangpinUtil::is_all_complete_pinyin(string pure_pinyin, string seg_pinyin)
{
    if (pure_pinyin.size() % 2)
        return false;
    auto pinyin_size = seg_pinyin.size();
    size_t index = 0;
    while (index < pinyin_size)
    {
        if (seg_pinyin[index] == '\'' || seg_pinyin[index + 1] == '\'')
            return false;
        index += 3;
    }
    return true;
}

/**
 * @brief Convert segmented shuangpin to segmented complete pinyin
 *
 * @param seg_shangpin Segmented shuangpin
 * @return string Segmented quanpin
 */
string ShuangpinUtil::convert_seg_shuangpin_to_seg_complete_pinyin(string seg_shangpin, const ShuangpinProfile &profile)
{
    vector<string> splitted_shuangpin;
    boost::split(splitted_shuangpin, seg_shangpin, boost::is_any_of("'"));
    string res = "";
    for (auto each : splitted_shuangpin)
    {
        if (each.size() == 1)
        {
            const std::string initial = find_source_by_code(profile.initials, each);
            res += (initial.empty() ? each : initial) + "'";
        }
        else if (each.size() == 2)
        {
            res += cvt_single_sp_to_pinyin(each, profile) + "'";
        }
    }
    return res.substr(0, res.size() - 1);
}

/**
 * @brief 判断是否是全码辅助
 *
 * @param pinyin
 * @return true 长度为偶数，且末尾两个辅助码中至少一个是大写字母，且前面都是完整的双拼
 * @return false
 */
bool ShuangpinUtil::IsFullHelpMode(std::string pinyin, const ShuangpinProfile &profile)
{
    auto len = pinyin.size();
    if (len == 0 || len == 2)
        return false;
    if (len % 2 != 0)
        return false;
    auto pure_pinyin = pinyin.substr(0, len - 2);
    if (is_all_complete_pinyin(pure_pinyin, pinyin_segmentation(pure_pinyin, profile)))
    {
        const bool first_helpcode_is_upper = pinyin[len - 2] >= 'A' && pinyin[len - 2] <= 'Z';
        const bool second_helpcode_is_upper = pinyin[len - 1] >= 'A' && pinyin[len - 1] <= 'Z';
        if (first_helpcode_is_upper || second_helpcode_is_upper)
        {
            return true;
        }
    }
    return false;
}

std::string ShuangpinUtil::GetFullHelpCodes(std::string pinyin)
{
    if (pinyin.size() < 2)
    {
        return "";
    }

    const bool reverse = pinyin[pinyin.size() - 2] >= 'A' && pinyin[pinyin.size() - 2] <= 'Z';
    std::string help_codes = pinyin.substr(pinyin.size() - 2, 2);
    std::transform(help_codes.begin(), help_codes.end(), help_codes.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (reverse)
    {
        std::swap(help_codes[0], help_codes[1]);
    }
    return help_codes;
}

namespace shuangpin
{

std::string get_local_appdata_path()
{
    return ShuangpinUtil::get_local_appdata_path();
}

std::string get_app_name()
{
    return ShuangpinUtil::app_name;
}

} // namespace shuangpin
