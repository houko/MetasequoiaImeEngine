#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace HelpcodeUtils
{

const std::unordered_map<std::string, std::string> &helpcode_keymap();

std::string get_first_han_char(const std::string &words);
std::string get_last_han_char(const std::string &words);
std::string::size_type count_han_chars(const std::string &words);
std::string::size_type count_utf8_chars(const std::string &text);
std::string compute_helpcodes(const std::string &words, bool uppercase_all = false);

bool is_quanpin_single_help_mode(const std::string &pinyin_with_cases);
bool is_quanpin_double_help_mode(const std::string &pinyin_with_cases);

template <typename TWordItem>
std::vector<TWordItem> reorder_candidates_with_single_helpcode(const std::vector<TWordItem> &candidate_list,
                                                               const std::string &help_code)
{
    std::vector<TWordItem> first_helpcode_matched_list;
    std::vector<TWordItem> last_helpcode_matched_list;
    std::vector<TWordItem> left_helpcode_matched_list;
    std::vector<TWordItem> result_list;
    const auto &keymap = helpcode_keymap();

    if (help_code.size() != 1)
    {
        return candidate_list;
    }

    for (const auto &cand : candidate_list)
    {
        const std::string &word = std::get<1>(cand);
        bool is_first_helpcode_matched = false;
        bool is_last_helpcode_matched = false;

        if (count_han_chars(word) == 1)
        {
            if (keymap.count(word))
            {
                if (keymap.at(word)[0] == help_code[0])
                {
                    first_helpcode_matched_list.push_back(cand);
                    is_first_helpcode_matched = true;
                }
            }

            if (!is_first_helpcode_matched)
            {
                if (keymap.count(word))
                {
                    if (keymap.at(word)[1] == help_code[0])
                    {
                        last_helpcode_matched_list.push_back(cand);
                        is_last_helpcode_matched = true;
                    }
                }
            }
        }
        else
        {
            const std::string first_han_char = get_first_han_char(word);
            const std::string last_han_char = get_last_han_char(word);

            if (keymap.count(first_han_char))
            {
                if (keymap.at(first_han_char)[0] == help_code[0])
                {
                    first_helpcode_matched_list.push_back(cand);
                    is_first_helpcode_matched = true;
                }
            }

            if (!is_first_helpcode_matched)
            {
                if (keymap.count(last_han_char))
                {
                    if (keymap.at(last_han_char)[0] == help_code[0])
                    {
                        last_helpcode_matched_list.push_back(cand);
                        is_last_helpcode_matched = true;
                    }
                }
            }
        }

        if (!is_first_helpcode_matched && !is_last_helpcode_matched)
        {
            left_helpcode_matched_list.push_back(cand);
        }
    }

    result_list.insert(result_list.end(), first_helpcode_matched_list.begin(), first_helpcode_matched_list.end());
    result_list.insert(result_list.end(), last_helpcode_matched_list.begin(), last_helpcode_matched_list.end());
    result_list.insert(result_list.end(), left_helpcode_matched_list.begin(), left_helpcode_matched_list.end());
    return result_list;
}

template <typename TWordItem>
std::vector<TWordItem> filter_candidates_with_double_helpcodes(const std::vector<TWordItem> &candidate_list,
                                                               const std::string &help_codes)
{
    std::vector<TWordItem> filtered_list;
    const auto &keymap = helpcode_keymap();
    if (help_codes.size() != 2)
    {
        return filtered_list;
    }

    for (const auto &cand : candidate_list)
    {
        const std::string &word = std::get<1>(cand);
        if (count_han_chars(word) == 1)
        {
            if (keymap.count(word) && keymap.at(word)[0] == help_codes[0] && keymap.at(word)[1] == help_codes[1])
            {
                filtered_list.push_back(cand);
            }
            continue;
        }

        const std::string first_han_char = get_first_han_char(word);
        const std::string last_han_char = get_last_han_char(word);
        if (keymap.count(first_han_char) && keymap.count(last_han_char) && keymap.at(first_han_char)[0] == help_codes[0] &&
            keymap.at(last_han_char)[0] == help_codes[1])
        {
            filtered_list.push_back(cand);
        }
    }

    return filtered_list;
}

} // namespace HelpcodeUtils
