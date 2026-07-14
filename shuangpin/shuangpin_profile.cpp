#include "shuangpin_profile.h"

const ShuangpinProfile &GetXiaoheShuangpinProfile()
{
    static const ShuangpinProfile profile{
        "xiaohe",
        {
            {"sh", "u"},
            {"ch", "i"},
            {"zh", "v"},
        },
        {
            {"a", "aa"},
            {"ai", "ai"},
            {"an", "an"},
            {"ao", "ao"},
            {"ang", "ah"},
            {"e", "ee"},
            {"ei", "ei"},
            {"en", "en"},
            {"eng", "eg"},
            {"er", "er"},
            {"o", "oo"},
            {"ou", "ou"},
        },
        {
            {"iu", "q"},
            {"ei", "w"},
            {"e", "e"},
            {"uan", "r"},
            {"ue", "t"},
            {"ve", "t"},
            {"un", "y"},
            {"u", "u"},
            {"i", "i"},
            {"uo", "o"},
            {"o", "o"},
            {"ie", "p"},
            {"a", "a"},
            {"ong", "s"},
            {"iong", "s"},
            {"ai", "d"},
            {"en", "f"},
            {"eng", "g"},
            {"ang", "h"},
            {"an", "j"},
            {"uai", "k"},
            {"ing", "k"},
            {"uang", "l"},
            {"iang", "l"},
            {"ou", "z"},
            {"ua", "x"},
            {"ia", "x"},
            {"ao", "c"},
            {"ui", "v"},
            {"v", "v"},
            {"in", "b"},
            {"iao", "n"},
            {"ian", "m"},
        },
    };
    return profile;
}

const ShuangpinProfile &GetZiranmaShuangpinProfile()
{
    static const ShuangpinProfile profile{
        "ziranma",
        {
            {"sh", "u"},
            {"ch", "i"},
            {"zh", "v"},
        },
        {
            {"a", "aa"},
            {"ai", "ai"},
            {"an", "an"},
            {"ao", "ao"},
            {"ang", "ah"},
            {"e", "ee"},
            {"ei", "ei"},
            {"en", "en"},
            {"eng", "eg"},
            {"er", "er"},
            {"o", "oo"},
            {"ou", "ou"},
        },
        {
            {"iu", "q"},
            {"ia", "w"},
            {"ua", "w"},
            {"e", "e"},
            {"uan", "r"},
            {"ue", "t"},
            {"ve", "t"},
            {"ing", "y"},
            {"uai", "y"},
            {"u", "u"},
            {"i", "i"},
            {"o", "o"},
            {"uo", "o"},
            {"un", "p"},
            {"a", "a"},
            {"iong", "s"},
            {"ong", "s"},
            {"iang", "d"},
            {"uang", "d"},
            {"en", "f"},
            {"eng", "g"},
            {"ang", "h"},
            {"an", "j"},
            {"ao", "k"},
            {"ai", "l"},
            {"ei", "z"},
            {"ie", "x"},
            {"iao", "c"},
            {"ui", "v"},
            {"v", "v"},
            {"ou", "b"},
            {"in", "n"},
            {"ian", "m"},
        },
    };
    return profile;
}

const ShuangpinProfile &GetShuangpinProfile(std::string_view name)
{
    return name == "ziranma" ? GetZiranmaShuangpinProfile() : GetXiaoheShuangpinProfile();
}
