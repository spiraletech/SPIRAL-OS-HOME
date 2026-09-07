#include "home/calendar.hpp"

#include <chrono>

namespace home {

CalendarState resolve_calendar(WorldTime time) noexcept {
    using namespace std::chrono;
    constexpr std::uint64_t kDayMs = 86400000;
    constexpr std::uint64_t kHourMs = 3600000;
    constexpr std::uint64_t kMinuteMs = 60000;
    constexpr std::uint64_t kSecondMs = 1000;

    const std::uint64_t day_index = time.milliseconds / kDayMs;
    std::uint64_t rest = time.milliseconds % kDayMs;

    const sys_days epoch = sys_days{year{2026}/January/1};
    const sys_days current = epoch + days{static_cast<days::rep>(day_index)};
    const year_month_day ymd{current};
    const weekday wd{current};

    const unsigned month = static_cast<unsigned>(ymd.month());
    Season season = Season::Winter;
    if (month >= 3 && month <= 5) season = Season::Spring;
    else if (month >= 6 && month <= 8) season = Season::Summer;
    else if (month >= 9 && month <= 11) season = Season::Autumn;

    CalendarState state{};
    state.date = CalendarDate{static_cast<int>(ymd.year()), month, static_cast<unsigned>(ymd.day())};
    state.hour = static_cast<unsigned>(rest / kHourMs); rest %= kHourMs;
    state.minute = static_cast<unsigned>(rest / kMinuteMs); rest %= kMinuteMs;
    state.second = static_cast<unsigned>(rest / kSecondMs);
    state.weekday = wd.c_encoding();
    state.day_index = day_index;
    state.season = season;
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
