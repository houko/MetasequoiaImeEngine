#include "quanpin_scheme.h"
#include "../common/helpcode_utils.h"
#include "../quanpin/quanpin_query.h"
#include "../shuangpin/shuangpin_query.h"
#include <cctype>

namespace
{
bool is_alpha_vk(UINT vk)
{
    return vk >= 'A' && vk <= 'Z';
}
} // namespace

void QuanpinScheme::reset()
{
    raw_input_.clear();
    key_strokes_.clear();
}

void QuanpinScheme::handle_key(UINT vk, UINT modifiers_down, WCHAR wch)
{
    if (vk == VK_BACK)
    {
        if (!raw_input_.empty())
        {
            raw_input_.pop_back();
        }
        if (!key_strokes_.empty())
        {
            key_strokes_.pop_back();
        }
        return;
    }

    if (vk == VK_ESCAPE || vk == VK_RETURN)
    {
        reset();
        return;
    }

    if (vk == VK_OEM_7)
    {
        key_strokes_.push_back(KeyStroke{vk, modifiers_down, wch});
        raw_input_.push_back('\'');
        return;
    }

    if (!is_alpha_vk(vk))
    {
        return;
    }

    key_strokes_.push_back(KeyStroke{vk, modifiers_down, wch});
    if (wch >= L'A' && wch <= L'Z')
    {
        raw_input_.push_back(static_cast<char>(wch));
    }
    else if (wch >= L'a' && wch <= L'z')
    {
        raw_input_.push_back(static_cast<char>(wch));
    }
    else
    {
        raw_input_.push_back(static_cast<char>(vk + ('a' - 'A')));
    }
}

QueryRequest QuanpinScheme::build_request() const
{
    QueryRequest request;
    request.scheme = type();
    request.raw_input_with_cases = raw_input_;
    request.raw_input.reserve(raw_input_.size());
    for (const char ch : raw_input_)
    {
        request.raw_input.push_back(
            ch == '\'' ? ch : static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    request.key_strokes = key_strokes_;

    std::string normalized_source = request.raw_input;
    size_t helpcode_length = 0;
    if (HelpcodeUtils::is_quanpin_double_help_mode(request.raw_input_with_cases) && normalized_source.size() >= 2)
    {
        helpcode_length = 2;
    }
    else if (HelpcodeUtils::is_quanpin_single_help_mode(request.raw_input_with_cases) && !normalized_source.empty())
    {
        helpcode_length = 1;
    }

    if (helpcode_length > 0 && normalized_source.size() >= helpcode_length)
    {
        normalized_source = normalized_source.substr(0, normalized_source.size() - helpcode_length);
    }

    request.normalized_input.reserve(normalized_source.size());
    for (const char ch : normalized_source)
    {
        if (ch != '\'')
        {
            request.normalized_input.push_back(ch);
        }
    }
    if (!request.normalized_input.empty())
    {
        try
        {
            const auto segments = quanpin::cut_pinyin_by_mode(normalized_source, "correction");
            if (!segments.empty())
            {
                request.normalized_segmentation = quanpin::join_segments(segments.front());
            }
        }
        catch (...)
        {
        }
    }

    if (request.normalized_segmentation.empty())
    {
        request.normalized_segmentation = normalized_source;
    }

    request.raw_segmentation = shuangpin::apply_segmentation_cases(request.normalized_segmentation, normalized_source);
    if (helpcode_length > 0)
    {
        request.raw_segmentation += "'";
        request.raw_segmentation += request.raw_input_with_cases.substr(request.raw_input_with_cases.size() - helpcode_length,
                                                                        helpcode_length);
    }
    request.segmentation = request.normalized_segmentation;

    request.valid = !request.normalized_input.empty();
    return request;
}

std::string QuanpinScheme::get_preedit() const
{
    return raw_input_;
}

SchemeType QuanpinScheme::type() const
{
    return SchemeType::Quanpin;
}
