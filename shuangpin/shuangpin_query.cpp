#include "shuangpin_query.h"

#include "../common/helpcode_utils.h"
#include "shuangpin_utils.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>

namespace shuangpin
{

namespace
{
std::string segment_chunk(const std::string &chunk)
{
    return chunk.empty() ? std::string{} : ShuangpinUtil::pinyin_segmentation(chunk);
}
}

std::string segment_input(const std::string &raw_input)
{
    if (raw_input.empty())
    {
        return {};
    }

    std::string result;
    size_t segment_start = 0;
    bool first_segment = true;
    while (segment_start <= raw_input.size())
    {
        const size_t separator = raw_input.find('\'', segment_start);
        const std::string chunk = separator == std::string::npos ? raw_input.substr(segment_start)
                                                                 : raw_input.substr(segment_start, separator - segment_start);
        if (!chunk.empty())
        {
            if (!first_segment)
            {
                result.push_back('\'');
            }
            result += segment_chunk(chunk);
            first_segment = false;
        }

        if (separator == std::string::npos)
        {
            break;
        }
        segment_start = separator + 1;
    }

    return result;
}

std::string to_quanpin_segmentation(const std::string &segmented_input)
{
    return ShuangpinUtil::convert_seg_shuangpin_to_seg_complete_pinyin(segmented_input);
}

std::string normalize_input_with_delimiters(const std::string &raw_input)
{
    return to_quanpin_segmentation(segment_input(raw_input));
}

std::string remove_manual_delimiters(const std::string &text)
{
    return boost::replace_all_copy(text, "'", "");
}

std::string normalize_input(const std::string &raw_input)
{
    return remove_manual_delimiters(normalize_input_with_delimiters(raw_input));
}

size_t effective_input_length(const std::string &raw_input)
{
    return remove_manual_delimiters(raw_input).size();
}

bool is_complete_input(const std::string &raw_input)
{
    if (raw_input.empty() || raw_input.front() == '\'' || raw_input.back() == '\'' || raw_input.find("''") != std::string::npos)
    {
        return false;
    }

    size_t segment_start = 0;
    while (segment_start <= raw_input.size())
    {
        const size_t separator = raw_input.find('\'', segment_start);
        const std::string chunk = separator == std::string::npos ? raw_input.substr(segment_start)
                                                                 : raw_input.substr(segment_start, separator - segment_start);
        if (chunk.empty() || !ShuangpinUtil::is_all_complete_pinyin(chunk, ShuangpinUtil::pinyin_segmentation(chunk)))
        {
            return false;
        }
        if (separator == std::string::npos)
        {
            break;
        }
        segment_start = separator + 1;
    }
    return true;
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

    if (extracted_input != boost::algorithm::to_lower_copy(remove_manual_delimiters(raw_input_with_cases)))
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

        while (index < raw_input_with_cases.size() && raw_input_with_cases[index] == '\'')
        {
            ++index;
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
    return HelpcodeUtils::get_first_han_char(words);
}

std::string get_last_han_char(const std::string &words)
{
    return HelpcodeUtils::get_last_han_char(words);
}

std::string::size_type count_utf8_chars(const std::string &text)
{
    return ShuangpinUtil::count_utf8_chars(text);
}

std::string::size_type count_han_chars(const std::string &text)
{
    return HelpcodeUtils::count_han_chars(text);
}

} // namespace shuangpin
