#include "shuangpin_query.h"

#include "shuangpin_utils.h"
#include <boost/algorithm/string/replace.hpp>

namespace shuangpin
{

std::string segment_input(const std::string &raw_input)
{
    return ShuangpinUtil::pinyin_segmentation(raw_input);
}

std::string to_quanpin_segmentation(const std::string &segmented_input)
{
    return ShuangpinUtil::convert_seg_shuangpin_to_seg_complete_pinyin(segmented_input);
}

std::string normalize_input(const std::string &raw_input)
{
    return boost::replace_all_copy(to_quanpin_segmentation(segment_input(raw_input)), "'", "");
}

std::string get_first_han_char(const std::string &words)
{
    return ShuangpinUtil::get_first_han_char(words);
}

std::string get_last_han_char(const std::string &words)
{
    return ShuangpinUtil::get_last_han_char(words);
}

std::string::size_type count_utf8_chars(const std::string &text)
{
    return ShuangpinUtil::count_utf8_chars(text);
}

std::string::size_type count_han_chars(const std::string &text)
{
    return ShuangpinUtil::cnt_han_chars(text);
}

} // namespace shuangpin
