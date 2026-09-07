#include "home/versioned_world.hpp"

#include <cassert>
#include <string_view>

int main() {
    using namespace home;
    const CalendarDate jan1{2026, 1, 1};
    const CalendarDate jan2{2026, 1, 2};
    const CalendarDate mar1{2026, 3, 1};

    const auto epoch = resolve_calendar(WorldTime{0});
    assert(epoch.date == jan1);
    assert(epoch.hour == 0 && epoch.minute == 0 && epoch.second == 0);
    assert(epoch.weekday == 4);
    assert(epoch.season == Season::Winter);
    const auto day_two = resolve_calendar(WorldTime{86400000});
    assert(day_two.date == jan2);
    assert(day_two.day_index == 1);
    const auto spring = resolve_calendar(WorldTime{59ULL * 86400000ULL});
    assert(spring.date == mar1);
    assert(spring.season == Season::Spring);
    VersionedWorld world{WorldId{9}};
    const auto advance = world.advance_time(43200000);
    assert(advance.ok());
    assert(world.calendar().date == jan2);
    assert(world.calendar().hour == 0);
    assert(std::string_view{season_name(Season::Summer)} == "summer");
    assert(std::string_view{season_name(Season::Autumn)} == "autumn");
    return 0;
}
