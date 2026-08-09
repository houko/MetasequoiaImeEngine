#include <utf8.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include "shuangpin_utils.h"

using namespace std;

const string ShuangpinUtil::app_name = "metasequoiaime";
static string path_seperator = "\\";
static string pinyin_file_name = "pinyin.txt";

/**
 * @brief Get the local app data path from environment variable LOCALAPPDATA
 *
 * @return string The path of local app data directory
 */
string ShuangpinUtil::get_local_appdata_path()
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
} // namespace

unordered_set<string> &initialize_quanpin_set()
{
    static unordered_set<string> tmp_set;
    ifstream pinyin_path(ShuangpinUtil::get_local_appdata_path() //
                         + path_seperator                        //
                         + ShuangpinUtil::app_name               //
                         + path_seperator                        //
                         + pinyin_file_name                      //
    );
    if (!pinyin_path.is_open())
    {
        (void)0;
    }
    string line;
    while (getline(pinyin_path, line))
    {
        line.erase(remove_if(line.begin(), line.end(), [](unsigned char x) { return isspace(x); }), line.end());
        tmp_set.insert(line);
    }
    return tmp_set;
}

unordered_set<string> &ShuangpinUtil::quanpin_set = initialize_quanpin_set();

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
        if (quanpin_set.count(sm + normalized_ym) > 0)
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
            if (quanpin_set.count(cvt_single_sp_to_pinyin(boost::algorithm::to_lower_copy(cur_sp), profile)) > 0)
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
 * @return true 是偶数个汉字，且最后一个拼音是大写字母，且前面的拼音都是完整的双拼
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
        if (pinyin[len - 1] >= 'A' && pinyin[len - 1] <= 'Z')
        {
            return true;
        }
    }
    return false;
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
