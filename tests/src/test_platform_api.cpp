#include "../../core/query_request.h"

#include <cstdint>
#include <type_traits>

static_assert(std::is_same_v<decltype(KeyStroke::vk), std::uint32_t>);
static_assert(std::is_same_v<decltype(KeyStroke::modifiers_down), std::uint32_t>);
static_assert(std::is_same_v<decltype(KeyStroke::wch), char16_t>);

int main()
{
    return 0;
}
