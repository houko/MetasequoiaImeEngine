#pragma once

#include "shuangpin_profile.h"
#include <string>

namespace shuangpin
{

std::string segment_input(const std::string &raw_input,
                          const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
std::string to_quanpin_segmentation(const std::string &segmented_input,
                                    const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
std::string normalize_input(const std::string &raw_input,
                            const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
std::string normalize_input_with_delimiters(const std::string &raw_input,
                                            const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
std::string remove_manual_delimiters(const std::string &text);
size_t effective_input_length(const std::string &raw_input);
size_t detect_active_double_helpcode_length(
    const std::string &raw_input, const std::string &raw_input_with_cases,
    const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
bool is_complete_input(const std::string &raw_input,
                       const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
std::string apply_segmentation_cases(const std::string &segmented_input, const std::string &raw_input_with_cases);
std::string get_first_han_char(const std::string &words);
std::string get_last_han_char(const std::string &words);
std::string::size_type count_utf8_chars(const std::string &text);
std::string::size_type count_han_chars(const std::string &text);

} // namespace shuangpin
