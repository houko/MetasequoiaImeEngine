#pragma once

#include <string>
#include <unordered_map>

struct ShuangpinProfile
{
    std::string name;
    std::unordered_map<std::string, std::string> initials;
    std::unordered_map<std::string, std::string> zero_initials;
    std::unordered_map<std::string, std::string> finals;
};

// Profiles have static storage duration and can safely be shared by sessions.
const ShuangpinProfile &GetXiaoheShuangpinProfile();
