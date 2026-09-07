#include "home/versioned_world.hpp"

#include <cassert>
#include <string_view>

int main() {
    using namespace home;
    constexpr std::uint64_t kDayMs = 86400000ULL;

    const CalendarDate jan1{2026, 1, 1};
    const CalendarDate jan2{2026, 1, 2};
    const CalendarDate mar1{2026, 3, 1};

    // Default HOME epoch remains deterministic and backward compatible.
    const auto epoch = resolve_calendar(WorldTime{0});
    assert(epoch.date == jan1);
    assert(epoch.hour == 0 && epoch.minute == 0 && epoch.second == 0);
    assert(epoch.weekday == 4); // Thursday
    assert(epoch.day_index == 0);
    assert(epoch.season == Season::Winter);

    const auto end_of_day = resolve_calendar(WorldTime{kDayMs - 1000});
    assert(end_of_day.date == jan1);
    assert(end_of_day.hour == 23 && end_of_day.minute == 59 && end_of_day.second == 59);

    const auto day_two = resolve_calendar(WorldTime{kDayMs});
    assert(day_two.date == jan2);
    assert(day_two.hour == 0 && day_two.minute == 0 && day_two.second == 0);
    assert(day_two.day_index == 1);

    const auto spring = resolve_calendar(WorldTime{59ULL * kDayMs});
    assert(spring.date == mar1);
    assert(spring.season == Season::Spring);

    // Gregorian validity and leap-year rollover are explicit L8 laws.
    assert(valid_calendar_date(CalendarDate{2028, 2, 29}));
    assert(!valid_calendar_date(CalendarDate{2027, 2, 29}));
    assert(!valid_calendar_date(CalendarDate{2026, 13, 1}));

    const CalendarConfig leap_epoch{CalendarDate{2028, 2, 28}};
    assert(resolve_calendar(WorldTime{kDayMs}, leap_epoch).date == CalendarDate(2028, 2, 29));
    assert(resolve_calendar(WorldTime{2 * kDayMs}, leap_epoch).date == CalendarDate(2028, 3, 1));

    const CalendarConfig year_end{CalendarDate{2027, 12, 31}};
    const auto new_year = resolve_calendar(WorldTime{kDayMs}, year_end);
    assert(new_year.date == CalendarDate(2028, 1, 1));
    assert(new_year.season == Season::Winter);

    // Meteorological season boundaries are deterministic for the HOME calendar.
    assert(resolve_calendar(WorldTime{0}, CalendarConfig{CalendarDate{2026, 3, 1}}).season == Season::Spring);
    assert(resolve_calendar(WorldTime{0}, CalendarConfig{CalendarDate{2026, 6, 1}}).season == Season::Summer);
    assert(resolve_calendar(WorldTime{0}, CalendarConfig{CalendarDate{2026, 9, 1}}).season == Season::Autumn);
    assert(resolve_calendar(WorldTime{0}, CalendarConfig{CalendarDate{2026, 12, 1}}).season == Season::Winter);
    assert(std::string_view{season_name(Season::Spring)} == "spring");
    assert(std::string_view{season_name(Season::Summer)} == "summer");
    assert(std::string_view{season_name(Season::Autumn)} == "autumn");
    assert(std::string_view{season_name(Season::Winter)} == "winter");

    // Each save owns its local epoch. With the default 2x clock, 12 real hours == one HOME day.
    const CalendarConfig halloween_epoch{CalendarDate{2032, 10, 31}};
    VersionedWorld world{WorldId{9}, WorldClockConfig{}, halloween_epoch};
    assert(world.calendar_config() == halloween_epoch);
    assert(world.calendar().date == CalendarDate(2032, 10, 31));
    assert(world.calendar().season == Season::Autumn);

    const auto advance = world.advance_time(43200000ULL);
    assert(advance.ok());
    assert(world.calendar().date == CalendarDate(2032, 11, 1));
    assert(world.calendar().hour == 0);
    assert(world.calendar().day_index == 1);

    // The local calendar epoch is persisted with the clock and restored exactly.
    const WorldSnapshot saved = world.snapshot();
    assert(saved.calendar_config == halloween_epoch);
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().calendar_config == halloween_epoch);
    const auto restored = VersionedWorld::from_snapshot(decoded.value());
    assert(restored.ok());
    assert(restored.value().calendar_config() == halloween_epoch);
    assert(restored.value().calendar().date == CalendarDate(2032, 11, 1));
    assert(restored.value().revision() == world.revision());

    // Snapshot v1/v2 saves predate CalendarConfig and resolve to the historical default epoch.
    const auto legacy_v1 = decode_snapshot(
        "HOME_SNAPSHOT 1\nWORLD 2 0\nENTITIES 0\nZONES 0\nCONNECTIONS 0\nPLACEMENTS 0\nEND\n");
    assert(legacy_v1.ok());
    assert(legacy_v1.value().calendar_config == CalendarConfig{});

    const auto legacy_v2 = decode_snapshot(
        "HOME_SNAPSHOT 2\nWORLD 3 0\nCLOCK 30000 0 0\nENTITIES 0\nZONES 0\nCONNECTIONS 0\nPLACEMENTS 0\nEND\n");
    assert(legacy_v2.ok());
    assert(legacy_v2.value().calendar_config == CalendarConfig{});

    WorldSnapshot invalid = saved;
    invalid.calendar_config.epoch = CalendarDate{2027, 2, 29};
    const auto invalid_encode = encode_snapshot(invalid);
    assert(!invalid_encode.ok());
    assert(invalid_encode.error().code == ErrorCode::ValidationFailed);

    const auto invalid_decode = decode_snapshot(
        "HOME_SNAPSHOT 3\nWORLD 4 0\nCLOCK 30000 0 0\nCALENDAR 2027 2 29\nENTITIES 0\nZONES 0\nCONNECTIONS 0\nPLACEMENTS 0\nEND\n");
    assert(!invalid_decode.ok());
    assert(invalid_decode.error().code == ErrorCode::SerializationError);

    return 0;
}
