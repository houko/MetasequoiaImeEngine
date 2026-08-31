#pragma once

#include "scheme_type.h"
#include <Windows.h>
#include <string>
#include <vector>

struct KeyStroke
{
    UINT vk = 0;
    UINT modifiers_down = 0;
    WCHAR wch = 0;
};

struct QueryRequest
{
    SchemeType scheme = SchemeType::Quanpin;
    std::string raw_input;
    std::string raw_input_with_cases;
    std::string normalized_input;
    std::string raw_segmentation;
    std::string normalized_segmentation;
    std::string segmentation;
    bool enable_shuangpin_helpcode = false;
    bool enable_quanpin_helpcode = false;
    bool enable_quanpin_autocorrect = true;
    std::vector<KeyStroke> key_strokes;
    bool valid = false;
};
