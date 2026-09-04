#include "../../local_modes/date_time_query.h"

#include <array>
#include <stdexcept>
#include <string>

namespace
{
using metasequoia::local_modes::LocalDateTime;

void require(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

LocalDateTime sample_time()
{
    return {2026, 8, 9, 0, 14, 30, 0};
}

template <std::size_t Size>
void require_words(const std::vector<WordItem> &actual,
                   const std::array<const char *, Size> &expected,
                   const char *message)
{
    require(actual.size() == expected.size(), message);
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        require(actual[index].word == expected[index] &&
                    actual[index].source == CandidateSource::Generated &&
                    actual[index].weight == static_cast<std::int64_t>(expected.size() - index),
                message);
    }
}
} // namespace

int main()
{
    const LocalDateTime now = sample_time();
    const std::array<const char *, 17> expected_dates = {
        "2026年8月9日", "2026-08-09", "2026/08/09", "2026.08.09", "20260809",
        "26年8月9日", "8月9日", "08-09", "0809", "2026年8月9日 星期日", "8月9日 周日",
        "2026-08-09 Sun", "2026-08-09 14:30", "8月9日 14:30", "二〇二六年八月九日",
        "贰零贰陆年捌月零玖日", "丙午年六月二十七日",
    };
    for (const char *keyword : std::array<const char *, 3>{"rq", "riqi", "date"})
    {
        require_words(metasequoia::local_modes::query_date_time(keyword, &now), expected_dates,
                      "A date alias did not preserve the Windows candidate order.");
    }

    const std::array<const char *, 13> expected_times = {
        "14:30", "14:30:00", "1430", "143000", "下午2:30", "下午2点30分", "下午两点半",
        "2:30 PM", "2:30pm", "02:30:00 PM", "2026-08-09 14:30:00",
        "2026年8月9日 14:30", "8月9日 下午2:30",
    };
    for (const char *keyword : std::array<const char *, 3>{"sj", "shijian", "time"})
    {
        require_words(metasequoia::local_modes::query_date_time(keyword, &now), expected_times,
                      "A time alias did not preserve the Windows candidate order.");
    }

    const std::array<const char *, 4> expected_sunday = {"星期日", "星期天", "Sunday", "Sun"};
    for (const char *keyword : std::array<const char *, 3>{"xq", "xingqi", "week"})
    {
        require_words(metasequoia::local_modes::query_date_time(keyword, &now), expected_sunday,
                      "A weekday alias did not preserve the Windows candidate order.");
    }

    LocalDateTime monday = now;
    monday.weekday = 1;
    const std::array<const char *, 3> expected_monday = {"星期一", "Monday", "Mon"};
    require_words(metasequoia::local_modes::query_date_time("week", &monday), expected_monday,
                  "Monday candidates were formatted incorrectly.");

    require(!metasequoia::local_modes::is_date_time_keyword("today") &&
                metasequoia::local_modes::is_date_time_keyword("week"),
            "Date/time keyword recognition diverged from Windows.");
    require(metasequoia::local_modes::query_date_time("today", &now).empty() &&
                metasequoia::local_modes::query_date_time("rq", &now, 0).empty() &&
                metasequoia::local_modes::query_date_time("rq", &now, -1).empty() &&
                metasequoia::local_modes::query_date_time("rq", &now, 3).size() == 3,
            "Date/time query limit or unknown-keyword handling was incorrect.");
    return 0;
}
