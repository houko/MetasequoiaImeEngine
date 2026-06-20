#include "shuangpin_query.h"

#include "shuangpin_utils.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>

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

std::string apply_segmentation_cases(const std::string &segmented_input, const std::string &raw_input_with_cases)
{
    if (segmented_input.empty() || raw_input_with_cases.empty())
    {
        return {};
    }

    std::string extracted_input;
    extracted_input.reserve(segmented_input.size());
    for (const char ch : segmented_input)
    {
        if (ch != '\'')
        {
            extracted_input.push_back(ch);
        }
    }

    if (extracted_input != boost::algorithm::to_lower_copy(raw_input_with_cases))
    {
        return segmented_input;
    }

    std::string result;
    result.reserve(segmented_input.size());
    size_t index = 0;
    for (const char ch : segmented_input)
    {
        if (ch == '\'')
        {
            result.push_back(ch);
            continue;
        }

        if (index >= raw_input_with_cases.size())
        {
            return segmented_input;
        }

        const char cased = raw_input_with_cases[index];
        if (ch == cased || ch == cased + ('a' - 'A'))
        {
            result.push_back(cased);
        }
        else
        {
            result.push_back(ch);
        }
        ++index;
    }

    return result;
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
