#include "provider_registry.h"
#include <stdexcept>

ICandidateProvider &ProviderRegistry::resolve(SchemeType scheme_type)
{
    switch (scheme_type)
    {
    case SchemeType::Quanpin:
    case SchemeType::Shuangpin:
        return pinyin_provider_;
    case SchemeType::Wubi:
        throw std::runtime_error("Wubi provider is not implemented yet.");
    default:
        throw std::runtime_error("Unknown scheme type.");
    }
}

void ProviderRegistry::reset_cache(SchemeType scheme_type)
{
    switch (scheme_type)
    {
    case SchemeType::Quanpin:
    case SchemeType::Shuangpin:
        pinyin_provider_.reset_cache();
        return;
    case SchemeType::Wubi:
        throw std::runtime_error("Wubi provider is not implemented yet.");
    default:
        throw std::runtime_error("Unknown scheme type.");
    }
}
