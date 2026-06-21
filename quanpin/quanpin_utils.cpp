#include "quanpin_utils.h"

#include "../common/helpcode_utils.h"

namespace quanpin
{
namespace
{

std::vector<std::string> split(const std::string &text, char delimiter)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (true)
    {
        const size_t pos = text.find(delimiter, start);
        if (pos == std::string::npos)
        {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
}

bool is_complete_pinyin_part(const std::string &part)
{
    if (part.empty())
    {
        return false;
    }

    return !cut_one_piece_greedy(part, true).empty();
}

} // namespace

const std::vector<std::string> &intact_pinyin_list()
{
    static const std::vector<std::string> kList = {
        "a",     "ai",    "an",    "ang",   "ao",   "ba",    "bai",   "ban",   "bang",   "bao",  "bei",  "ben",
        "beng",  "bi",    "bian",  "biao",  "bie",  "bin",   "bing",  "bo",    "bu",     "ca",   "cai",  "can",
        "cang",  "cao",   "ce",    "cen",   "ceng", "cha",   "chai",  "chan",  "chang",  "chao", "che",  "chen",
        "cheng", "chi",   "chong", "chou",  "chu",  "chua",  "chuai", "chuan", "chuang", "chui", "chun", "chuo",
        "ci",    "cong",  "cou",   "cu",    "cuan", "cui",   "cun",   "cuo",   "da",     "dai",  "dan",  "dang",
        "dao",   "de",    "dei",   "den",   "deng", "di",    "dia",   "dian",  "diao",   "die",  "ding", "diu",
        "dong",  "dou",   "du",    "duan",  "dui",  "dun",   "duo",   "e",     "ei",     "en",   "er",   "fa",
        "fan",   "fang",  "fei",   "fen",   "feng", "fiao",  "fo",    "fou",   "fu",     "ga",   "gai",  "gan",
        "gang",  "gao",   "ge",    "gei",   "gen",  "geng",  "gong",  "gou",   "gu",     "gua",  "guai", "guan",
        "guang", "gui",   "gun",   "guo",   "ha",   "hai",   "han",   "hang",  "hao",    "he",   "hei",  "hen",
        "heng",  "hong",  "hou",   "hu",    "hua",  "huai",  "huan",  "huang", "hui",    "hun",  "huo",  "ji",
        "jia",   "jian",  "jiang", "jiao",  "jie",  "jin",   "jing",  "jiong", "jiu",    "ju",   "juan", "jue",
        "jun",   "jve",   "ka",    "kai",   "kan",  "kang",  "kao",   "ke",    "kei",    "ken",  "keng", "kong",
        "kou",   "ku",    "kua",   "kuai",  "kuan", "kuang", "kui",   "kun",   "kuo",    "la",   "lai",  "lan",
        "lang",  "lao",   "le",    "lei",   "leng", "li",    "lia",   "lian",  "liang",  "liao", "lie",  "lin",
        "ling",  "liu",   "lo",    "long",  "lou",  "lu",    "luan",  "lue",   "lun",    "luo",  "lv",   "lve",
        "ma",    "mai",   "man",   "mang",  "mao",  "me",    "mei",   "men",   "meng",   "mi",   "mian", "miao",
        "mie",   "min",   "ming",  "miu",   "mo",   "mou",   "mu",    "na",    "nai",    "nan",  "nang", "nao",
        "ne",    "nei",   "nen",   "neng",  "ni",   "nian",  "niang", "niao",  "nie",    "nin",  "ning", "niu",
        "nong",  "nou",   "nu",    "nuan",  "nue",  "nun",   "nuo",   "nv",    "nve",    "o",    "ou",   "pa",
        "pai",   "pan",   "pang",  "pao",   "pei",  "pen",   "peng",  "pi",    "pian",   "piao", "pie",  "pin",
        "ping",  "po",    "pou",   "pu",    "qi",   "qia",   "qian",  "qiang", "qiao",   "qie",  "qin",  "qing",
        "qiong", "qiu",   "qu",    "quan",  "que",  "qun",   "qve",   "ran",   "rang",   "rao",  "re",   "ren",
        "reng",  "ri",    "rong",  "rou",   "ru",   "ruan",  "rui",   "run",   "ruo",    "sa",   "sai",  "san",
        "sang",  "sao",   "se",    "sen",   "seng", "sha",   "shai",  "shan",  "shang",  "shao", "she",  "shei",
        "shen",  "sheng", "shi",   "shou",  "shu",  "shua",  "shuai", "shuan", "shuang", "shui", "shun", "shuo",
        "si",    "song",  "sou",   "su",    "suan", "sui",   "sun",   "suo",   "ta",     "tai",  "tan",  "tang",
        "tao",   "te",    "teng",  "ti",    "tian", "tiao",  "tie",   "ting",  "tong",   "tou",  "tu",   "tuan",
        "tui",   "tun",   "tuo",   "wa",    "wai",  "wan",   "wang",  "wei",   "wen",    "weng", "wo",   "wu",
        "xi",    "xia",   "xian",  "xiang", "xiao", "xie",   "xin",   "xing",  "xiong",  "xiu",  "xu",   "xuan",
        "xue",   "xun",   "xve",   "ya",    "yan",  "yang",  "yao",   "ye",    "yi",     "yin",  "ying", "yo",
        "yong",  "you",   "yu",    "yuan",  "yue",  "yun",   "yve",   "za",    "zai",    "zan",  "zang", "zao",
        "ze",    "zei",   "zen",   "zeng",  "zha",  "zhai",  "zhan",  "zhang", "zhao",   "zhe",  "zhei", "zhen",
        "zheng", "zhi",   "zhong", "zhou",  "zhu",  "zhua",  "zhuai", "zhuan", "zhuang", "zhui", "zhun", "zhuo",
        "zi",    "zong",  "zou",   "zu",    "zuan", "zui",   "zun",   "zuo"};
    return kList;
}

const std::unordered_set<std::string> &intact_pinyin_set()
{
    static const std::unordered_set<std::string> kSet(intact_pinyin_list().begin(), intact_pinyin_list().end());
    return kSet;
}

const std::unordered_set<std::string> &prefix_pinyin_set()
{
    static const std::unordered_set<std::string> kSet = [] {
        std::unordered_set<std::string> result;
        for (const auto &item : intact_pinyin_list())
        {
            for (size_t i = 1; i <= item.size(); ++i)
            {
                result.insert(item.substr(0, i));
            }
        }
        return result;
    }();
    return kSet;
}

std::vector<std::string> cut_one_piece_greedy(const std::string &pinyin, bool intact_only)
{
    const auto &pinyin_set = intact_only ? intact_pinyin_set() : prefix_pinyin_set();
    std::vector<std::string> result;
    size_t index = 0;
    while (index < pinyin.size())
    {
        std::string matched;
        for (size_t end = pinyin.size(); end > index; --end)
        {
            const auto piece = pinyin.substr(index, end - index);
            if (pinyin_set.find(piece) != pinyin_set.end())
            {
                matched = piece;
                break;
            }
        }
        if (matched.empty())
        {
            return {};
        }
        result.push_back(matched);
        index += matched.size();
    }
    return result;
}

bool is_complete_pinyin_input(const std::string &pinyin)
{
    if (pinyin.empty())
    {
        return false;
    }

    if (pinyin.find('\'') == std::string::npos)
    {
        return is_complete_pinyin_part(pinyin);
    }

    for (const auto &part : split(pinyin, '\''))
    {
        if (!is_complete_pinyin_part(part))
        {
            return false;
        }
    }

    return true;
}

size_t detect_active_helpcode_length(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    const auto &input_with_cases = raw_input_with_cases.empty() ? raw_input : raw_input_with_cases;
    if (HelpcodeUtils::is_quanpin_double_help_mode(input_with_cases) && raw_input.size() >= 2 &&
        is_complete_pinyin_input(raw_input.substr(0, raw_input.size() - 2)))
    {
        return 2;
    }

    if (HelpcodeUtils::is_quanpin_single_help_mode(input_with_cases) && !raw_input.empty() &&
        is_complete_pinyin_input(raw_input.substr(0, raw_input.size() - 1)))
    {
        return 1;
    }

    return 0;
}

std::string strip_active_helpcodes(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    const size_t helpcode_length = detect_active_helpcode_length(raw_input, raw_input_with_cases);
    if (helpcode_length == 0 || raw_input.size() < helpcode_length)
    {
        return raw_input;
    }
    return raw_input.substr(0, raw_input.size() - helpcode_length);
}

} // namespace quanpin
