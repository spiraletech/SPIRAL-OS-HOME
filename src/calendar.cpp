#include "home/calendar.hpp"

#include <chrono>

namespace home {

bool valid_calendar_date(CalendarDate date) noexcept {
    using namespace std::chrono;
    if (date.year < 1 || date.year > 9999 || date.month < 1 || date.month > 12 || date.day < 1 || date.day > 31) {
        return false;
    }
    return year_month_day{year{date.year}, month{date.month}, day{date.day}}.ok();
}

Season season_for_month(unsigned month) noexcept {
    if (month >= 3 && month <= 5) return Season::Spring;
    if (month >= 6 && month <= 8) return Season::Summer;
    if (month >= 9 && month <= 11) return Season::Autumn;
    return Season::Winter;
}

CalendarState resolve_calendar(WorldTime time, CalendarConfig config) noexcept {
    using namespace std::chrono;
    constexpr std::uint64_t kDayMs = 86400000;
    constexpr std::uint64_t kHourMs = 3600000;
    constexpr std::uint64_t kMinuteMs = 60000;
    constexpr std::uint64_t kSecondMs = 1000;

    if (!valid_calendar_date(config.epoch)) config = CalendarConfig{};

    const std::uint64_t day_index = time.milliseconds / kDayMs;
    std::uint64_t rest = time.milliseconds % kDayMs;

    const sys_days epoch = sys_days{year{config.epoch.year}/month{config.epoch.month}/day{config.epoch.day}};
    const sys_days current = epoch + days{static_cast<days::rep>(day_index)};
    const year_month_day ymd{current};
    const weekday wd{current};

    const unsigned resolved_month = static_cast<unsigned>(ymd.month());
    CalendarState state{};
    state.date = CalendarDate{static_cast<int>(ymd.year()), resolved_month, static_cast<unsigned>(ymd.day())};
    state.hour = static_cast<unsigned>(rest / kHourMs); rest %= kHourMs;
    state.minute = static_cast<unsigned>(rest / kMinuteMs); rest %= kMinuteMs;
    state.second = static_cast<unsigned>(rest / kSecondMs);
    state.weekday = wd.c_encoding();
    state.day_index = day_index;
    state.season = season_for_month(resolved_month);
    return state;
}

const char* season_name(Season season) noexcept {
    switch (season) {
        case Season::Winter: return "winter";
        case Season::Spring: return "spring";
        case Season::Summer: return "summer";
        case Season::Autumn: return "autumn";
    }
    return "winter";
}

} // namespace home
