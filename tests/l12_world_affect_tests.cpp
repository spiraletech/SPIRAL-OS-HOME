#include "home/versioned_world.hpp"

#include <cassert>
#include <string_view>
#include <variant>

int main() {
    using namespace home;

    const EventDefinition halloween{
        EventId{1}, "halloween", "Halloween", EventKind::Holiday,
        AnnualDateRule{10, 31, 1}, 10, {"calendar.halloween", "world.spooky"}};

    // Pure semantic resolution: Halloween + meaningful rain derives the first theme anchor.
    CalendarState halloween_date{};
    halloween_date.date = CalendarDate{2026, 10, 31};
    WeatherState storm{};
    storm.zone = ZoneId{1};
    storm.intensity = RainIntensity::Storm;
    const auto haunted = resolve_world_context(halloween_date, {halloween}, &storm);
    assert(haunted.ok());
    assert(haunted.value().affect.tone == WorldTone::Haunted);
    assert(haunted.value().anchor.anchor == ThemeAnchor::HauntedHalloweenRain);
    assert(haunted.value().anchor.source == AnchorSource::Derived);
    assert(haunted.value().anchor.authority.empty());
    assert(haunted.value().calendar.date == halloween_date.date);
    assert(storm.intensity == RainIntensity::Storm);

    CalendarState ordinary{};
    ordinary.date = CalendarDate{2026, 11, 8};
    const auto weather_only = resolve_world_context(ordinary, {}, &storm);
    assert(weather_only.ok());
    assert(weather_only.value().affect.tone == WorldTone::Tense);
    assert(weather_only.value().anchor.anchor == ThemeAnchor::None);
    assert(weather_only.value().anchor.source == AnchorSource::None);

    // Persistent anchor overrides require provenance; a bare boolean is not authority.
    const WorldAnchorOverride unauthorized{ThemeAnchor::HauntedHalloweenRain, 500, {}};
    const auto rejected_override = resolve_world_context(ordinary, {}, nullptr, unauthorized);
    assert(!rejected_override.ok());
    assert(rejected_override.error().code == ErrorCode::ValidationFailed);

    const WorldAnchorOverride authorized{ThemeAnchor::HauntedHalloweenRain, 500, "operator.theme-anchor"};
    const auto forced = resolve_world_context(ordinary, {}, nullptr, authorized);
    assert(forced.ok());
    assert(forced.value().anchor.anchor == ThemeAnchor::HauntedHalloweenRain);
    assert(forced.value().anchor.source == AnchorSource::AuthorizedOverride);
    assert(forced.value().anchor.authority == "operator.theme-anchor");
    assert(forced.value().affect.tone == WorldTone::Haunted);
    assert(forced.value().affect.intensity_permille >= 500);
    assert(forced.value().calendar.date == ordinary.date);

    const WorldAnchorOverride authorized_clear{ThemeAnchor::None, 0, "operator.clear-anchor"};
    const auto cleared = resolve_world_context(halloween_date, {halloween}, &storm, authorized_clear);
    assert(cleared.ok());
    assert(cleared.value().anchor.anchor == ThemeAnchor::None);
    assert(cleared.value().anchor.source == AnchorSource::AuthorizedOverride);
    assert(cleared.value().anchor.authority == "operator.clear-anchor");

    WorldAffectState invalid_affect{};
    invalid_affect.valence_milli = 1001;
    assert(!validate_world_affect_state(invalid_affect).ok());
    const WorldAnchorState invalid_derived{ThemeAnchor::HauntedHalloweenRain, 500, AnchorSource::Derived, "not-allowed"};
    assert(!validate_world_anchor_state(invalid_derived).ok());

    // HOME integration: affect/anchor are canonical, revisioned truth.
    VersionedWorld world{WorldId{12}, WorldClockConfig{}, CalendarConfig{CalendarDate{2026, 10, 31}}};
    ZoneCreateInfo zone_info{};
    zone_info.kind = ZoneKind::District;
    zone_info.key = "mission-bay-coast";
    zone_info.display_name = "Mission Bay Coast";
    const auto zone = world.create_zone(zone_info);
    assert(zone.ok());
    assert(world.add_event_definition(halloween).ok());

    const ClimateProfile soaked_coast{zone.value(), 18000, 1000, 300, 12345};
    assert(world.set_climate_profile(soaked_coast).ok());
    const auto rain = world.evolve_zone_weather(zone.value(), 77);
    assert(rain.ok());
    assert(rain.value().intensity >= RainIntensity::Rain);
    const WorldRevision before_context = world.revision();

    const auto derived = world.refresh_world_context(zone.value());
    assert(derived.ok());
    assert(world.revision() == WorldRevision{before_context.value() + 1});
    assert(world.affect().tone == WorldTone::Haunted);
    assert(world.anchor().anchor == ThemeAnchor::HauntedHalloweenRain);
    assert(world.anchor().source == AnchorSource::Derived);
    assert(world.history().back().changes().size() == 2);
    assert(world.history().back().changes()[0].kind == WorldChangeKind::WorldAffectChanged);
    assert(world.history().back().changes()[1].kind == WorldChangeKind::WorldAnchorChanged);
    const auto& affect_delta = std::get<WorldAffectChanged>(world.history().back().changes()[0].payload);
    const auto& anchor_delta = std::get<WorldAnchorChanged>(world.history().back().changes()[1].payload);
    assert(affect_delta.after == world.affect());
    assert(anchor_delta.after == world.anchor());

    const WorldRevision before_noop = world.revision();
    const auto noop = world.refresh_world_context(zone.value());
    assert(!noop.ok());
    assert(noop.error().code == ErrorCode::ValidationFailed);
    assert(world.revision() == before_noop);

    const auto missing_weather = world.refresh_world_context(ZoneId{999});
    assert(!missing_weather.ok());
    assert(world.revision() == before_noop);

    // Advance one HOME day: Halloween is no longer active. Natural refresh clears the anchor.
    const auto next_day = world.advance_time(43'200'000);
    assert(next_day.ok());
    assert(world.calendar().date == CalendarDate{2026, 11, 1});
    const auto natural_november = world.refresh_world_context(zone.value());
    assert(natural_november.ok());
    assert(world.anchor().anchor == ThemeAnchor::None);
    assert(world.anchor().source == AnchorSource::None);
    assert(world.affect().tone != WorldTone::Haunted);

    // Authorized persistent theme anchor can intentionally override the natural November context.
    const WorldRevision before_authorized = world.revision();
    const auto canonical_forced = world.refresh_world_context(zone.value(), authorized);
    assert(canonical_forced.ok());
    assert(world.revision() == WorldRevision{before_authorized.value() + 1});
    assert(world.anchor().anchor == ThemeAnchor::HauntedHalloweenRain);
    assert(world.anchor().source == AnchorSource::AuthorizedOverride);
    assert(world.anchor().authority == "operator.theme-anchor");
    assert(world.affect().tone == WorldTone::Haunted);

    // Snapshot v6 persists actual affect/anchor truth and authority, not any L10 selector.
    const WorldSnapshot saved = world.snapshot();
    assert(saved.affect == world.affect());
    assert(saved.anchor == world.anchor());
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    assert(encoded.value().starts_with("HOME_SNAPSHOT "));
    assert(encoded.value().find("operator.theme-anchor") != std::string::npos);
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().affect == saved.affect);
    assert(decoded.value().anchor == saved.anchor);
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    const VersionedWorld restored = restored_result.value();
    assert(restored.revision() == world.revision());
    assert(restored.affect() == world.affect());
    assert(restored.anchor() == world.anchor());
    const auto reencoded = encode_snapshot(restored.snapshot());
    assert(reencoded.ok());
    assert(reencoded.value() == encoded.value());

    // v5 saves remain readable and restore with neutral unresolved L12 state.
    const auto legacy_v5 = decode_snapshot(
        "HOME_SNAPSHOT 5\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "CLIMATES 0\n"
        "WEATHER 0\n"
        "ENTITIES 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(legacy_v5.ok());
    assert(legacy_v5.value().affect == WorldAffectState{});
    assert(legacy_v5.value().anchor == WorldAnchorState{});

    // v6 rejects forged override state with no authority provenance.
    const auto forged = decode_snapshot(
        "HOME_SNAPSHOT 6\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "CLIMATES 0\n"
        "WEATHER 0\n"
        "AFFECT 0 0 0 1000\n"
        "ANCHOR 1 500 2 \"\"\n"
        "ENTITIES 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(!forged.ok());

    assert(std::string_view{restored.anchor().authority} == "operator.theme-anchor");
    return 0;
}
