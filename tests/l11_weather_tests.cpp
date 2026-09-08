#include "home/versioned_world.hpp"

#include <cassert>
#include <limits>
#include <variant>

int main() {
    using namespace home;

    // Pure weather evolution is deterministic for the same climate, memory and entropy.
    const ClimateProfile coast{ZoneId{1}, 18000, 700, 300, 12345};
    WeatherState start = initial_weather_state(coast);
    const auto a = evolve_weather(start, coast, Season::Winter, 77);
    const auto b = evolve_weather(start, coast, Season::Winter, 77);
    assert(a.ok() && b.ok());
    assert(a.value() == b.value());
    assert(a.value().sequence == 1);

    // Location climate affects the semantic result even under identical entropy.
    const ClimateProfile dry_coast{ZoneId{1}, 18000, 100, 300, 12345};
    const auto dry = evolve_weather(initial_weather_state(dry_coast), dry_coast, Season::Winter, 77);
    assert(dry.ok());
    assert(dry.value().precipitation_permille <= a.value().precipitation_permille);

    // Weather has memory: prior intensity/age participates in the next result.
    WeatherState rainy = a.value();
    rainy.intensity = RainIntensity::Storm;
    rainy.age_minutes = 12;
    const auto memory = evolve_weather(rainy, coast, Season::Winter, 77);
    assert(memory.ok());
    assert(memory.value().previous_intensity == RainIntensity::Storm);
    assert(memory.value().sequence == rainy.sequence + 1);

    // Overflow is rejected atomically by the pure evolution layer.
    WeatherState exhausted = start;
    exhausted.sequence = std::numeric_limits<std::uint64_t>::max();
    const auto overflow = evolve_weather(exhausted, coast, Season::Winter, 1);
    assert(!overflow.ok());
    assert(overflow.error().code == ErrorCode::Overflow);

    ClimateCatalog climates;
    assert(climates.set(coast).ok());
    assert(climates.find(ZoneId{1}) != nullptr);
    assert(climates.snapshot().size() == 1);

    WeatherLedger ledger;
    assert(ledger.set(a.value()).ok());
    assert(ledger.find(ZoneId{1}) != nullptr);
    assert(ledger.snapshot().size() == 1);

    // HOME integration: climate and weather are canonical revisioned truth.
    VersionedWorld world{WorldId{11}};
    ZoneCreateInfo coastal_zone_info{};
    coastal_zone_info.kind = ZoneKind::District;
    coastal_zone_info.key = "mission-bay-coast";
    coastal_zone_info.display_name = "Mission Bay Coast";
    const auto coastal_zone = world.create_zone(coastal_zone_info);
    assert(coastal_zone.ok());

    ZoneCreateInfo inland_zone_info{};
    inland_zone_info.kind = ZoneKind::District;
    inland_zone_info.key = "inland";
    inland_zone_info.display_name = "Inland";
    const auto inland_zone = world.create_zone(inland_zone_info);
    assert(inland_zone.ok());
    assert(world.revision() == WorldRevision{2});

    const ClimateProfile wet_profile{coastal_zone.value(), 18000, 900, 250, 777};
    const ClimateProfile dry_profile{inland_zone.value(), 18000, 100, 250, 777};
    assert(world.set_climate_profile(wet_profile).ok());
    assert(world.set_climate_profile(dry_profile).ok());
    assert(world.revision() == WorldRevision{4});
    assert(world.climates().snapshot().size() == 2);

    const WorldRevision before_noop = world.revision();
    const auto no_change = world.set_climate_profile(wet_profile);
    assert(!no_change.ok());
    assert(world.revision() == before_noop);

    ClimateProfile missing_zone = wet_profile;
    missing_zone.zone = ZoneId{999};
    const auto missing_climate = world.set_climate_profile(missing_zone);
    assert(!missing_climate.ok());
    assert(world.revision() == before_noop);

    const auto wet_weather = world.evolve_zone_weather(coastal_zone.value(), 42);
    assert(wet_weather.ok());
    assert(world.revision() == WorldRevision{5});
    assert(world.weather().find(coastal_zone.value()) != nullptr);
    assert(world.history().back().changes().size() == 1);
    assert(world.history().back().changes().front().kind == WorldChangeKind::WeatherStateChanged);
    const auto& weather_change = std::get<WeatherStateChanged>(world.history().back().changes().front().payload);
    assert(!weather_change.before.has_value());
    assert(weather_change.after == wet_weather.value());
    assert(weather_change.entropy == 42);
    assert(weather_change.season == Season::Winter);

    const auto dry_weather = world.evolve_zone_weather(inland_zone.value(), 42);
    assert(dry_weather.ok());
    assert(world.revision() == WorldRevision{6});
    assert(dry_weather.value().precipitation_permille <= wet_weather.value().precipitation_permille);

    // Re-evolving uses the saved prior weather state rather than restarting from climate defaults.
    const WeatherState wet_before_second = *world.weather().find(coastal_zone.value());
    const auto wet_second = world.evolve_zone_weather(coastal_zone.value(), 42);
    assert(wet_second.ok());
    assert(wet_second.value().sequence == wet_before_second.sequence + 1);
    const auto& second_change = std::get<WeatherStateChanged>(world.history().back().changes().front().payload);
    assert(second_change.before.has_value());
    assert(*second_change.before == wet_before_second);

    // An existing zone without climate cannot mint weather or consume a revision.
    ZoneCreateInfo no_climate_info{};
    no_climate_info.kind = ZoneKind::District;
    no_climate_info.key = "no-climate";
    no_climate_info.display_name = "No Climate";
    const auto no_climate_zone = world.create_zone(no_climate_info);
    assert(no_climate_zone.ok());
    const WorldRevision before_missing_weather = world.revision();
    const auto missing_weather = world.evolve_zone_weather(no_climate_zone.value(), 1);
    assert(!missing_weather.ok());
    assert(missing_weather.error().code == ErrorCode::NotFound);
    assert(world.revision() == before_missing_weather);

    // Snapshot v5 preserves climate configuration and exact weather memory.
    const WorldSnapshot saved = world.snapshot();
    assert(saved.climates.size() == 2);
    assert(saved.weather.size() == 2);
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    const VersionedWorld restored = restored_result.value();
    assert(restored.revision() == world.revision());
    assert(restored.climates().snapshot() == world.climates().snapshot());
    assert(restored.weather().snapshot() == world.weather().snapshot());
    const auto reencoded = encode_snapshot(restored.snapshot());
    assert(reencoded.ok());
    assert(reencoded.value() == encoded.value());

    // v4 saves remain readable and naturally restore with no L11 climate/weather state.
    const auto legacy = decode_snapshot(
        "HOME_SNAPSHOT 4\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "ENTITIES 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(legacy.ok());
    assert(legacy.value().climates.empty());
    assert(legacy.value().weather.empty());

    // v5 rejects weather that has no climate authority.
    const auto orphan_weather = decode_snapshot(
        "HOME_SNAPSHOT 5\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "CLIMATES 0\n"
        "WEATHER 1\n"
        "W 1 0 0 18000 0 0 0 0 0\n"
        "ENTITIES 0\n"
        "ZONES 1\n"
        "Z 1 1 1 0 \"zone\" \"Zone\"\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(!orphan_weather.ok());

    return 0;
}
