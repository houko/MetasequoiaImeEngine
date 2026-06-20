#pragma once

#include <string>

namespace shuangpin
{

std::string segment_input(const std::string &raw_input);
std::string to_quanpin_segmentation(const std::string &segmented_input);
std::string normalize_input(const std::string &raw_input);
std::string apply_segmentation_cases(const std::string &segmented_input, const std::string &raw_input_with_cases);
std::string get_first_han_char(const std::string &words);
std::string get_last_han_char(const std::string &words);
std::string::size_type count_utf8_chars(const std::string &text);
std::string::size_type count_han_chars(const std::string &text);

} // namespace shuangpin
