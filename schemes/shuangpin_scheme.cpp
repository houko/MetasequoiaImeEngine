#include "shuangpin_scheme.h"
#include "../shuangpin/shuangpin_query.h"
#include <cctype>

namespace
{
bool is_alpha_vk(UINT vk)
{
    return vk >= 'A' && vk <= 'Z';
}
} // namespace

ShuangpinScheme::ShuangpinScheme(const ShuangpinProfile &profile) : profile_(profile)
{
}

void ShuangpinScheme::reset()
{
    raw_input_.clear();
    key_strokes_.clear();
}

void ShuangpinScheme::set_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    raw_input_ = raw_input_with_cases.empty() ? raw_input : raw_input_with_cases;
    key_strokes_.clear();
}

void ShuangpinScheme::handle_key(UINT vk, UINT modifiers_down, WCHAR wch)
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
        if (raw_input_.empty() || raw_input_.back() != '\'')
        {
            key_strokes_.push_back(KeyStroke{vk, modifiers_down, wch});
            raw_input_.push_back('\'');
        }
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

QueryRequest ShuangpinScheme::build_request() const
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
    request.valid = shuangpin::effective_input_length(request.raw_input) > 0;

    if (!request.valid)
    {
        return request;
    }

    const std::string raw_segmentation = shuangpin::segment_input(request.raw_input, profile_);
    request.raw_segmentation = shuangpin::apply_segmentation_cases(raw_segmentation, request.raw_input_with_cases);
    request.normalized_segmentation = shuangpin::to_quanpin_segmentation(raw_segmentation, profile_);
    request.segmentation = request.normalized_segmentation;
    request.normalized_input = shuangpin::normalize_input(request.raw_input, profile_);
    return request;
}

std::string ShuangpinScheme::get_preedit() const
{
    return raw_input_;
}

SchemeType ShuangpinScheme::type() const
{
    return SchemeType::Shuangpin;
}
