#include "home/versioned_world.hpp"

#include <cassert>

int main() {
    using namespace home;

    const auto epoch = resolve_calendar(WorldTime{0});
    assert(epoch.date == CalendarDate{2026, 1, 1});
    assert(epoch.hour == 0 && epoch.minute == 0 && epoch.second == 0);
    assert(epoch.weekday == 4); // Thursday
    assert(epoch.season == Season::Winter);

    const auto day_two = resolve_calendar(WorldTime{86400000});
    assert(day_two.date == CalendarDate{2026, 1, 2});
    assert(day_two.day_index == 1);

    const auto spring = resolve_calendar(WorldTime{59ULL * 86400000ULL});
    assert(spring.date == CalendarDate{2026, 3, 1});
    assert(spring.season == Season::Spring);

    VersionedWorld world{WorldId{9}};
    const auto advance = world.advance_time(43200000); // 12 real hours = 24 HOME hours
    assert(advance.ok());
    assert(world.calendar().date == CalendarDate{2026, 1, 2});
    assert(world.calendar().hour == 0);

    assert(std::string_view{season_name(Season::Summer)} == "summer");
    assert(std::string_view{season_name(Season::Autumn)} == "autumn");
    return 0;
}
