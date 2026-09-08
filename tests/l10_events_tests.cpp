#include "home/versioned_world.hpp"

#include <cassert>
#include <string_view>
#include <vector>

int main() {
    using namespace home;

    const EventDefinition halloween{
        EventId{1}, "halloween", "Halloween", EventKind::Holiday,
        AnnualDateRule{10, 31, 1}, 10, {"calendar.halloween", "world.spooky"}};
    const EventDefinition year_turn{
        EventId{2}, "year_turn", "Year Turn", EventKind::Festival,
        AnnualDateRule{12, 31, 2}, 5, {"calendar.new_year"}};
    const EventDefinition leap_watch{
        EventId{3}, "leap_watch", "Leap Watch", EventKind::Observance,
        AnnualDateRule{2, 29, 2}, 7, {"calendar.leap"}};
    const EventDefinition harvest{
        EventId{4}, "harvest", "Harvest Festival", EventKind::Festival,
        AnnualDateRule{10, 31, 2}, 20, {"season.autumn"}};
    const std::vector<EventDefinition> defs{halloween, year_turn, leap_watch, harvest};

    // Calendar-derived windows and deterministic priority ordering.
    CalendarState oct{};
    oct.date = CalendarDate{2026, 10, 31};
    const auto active = resolve_event_set(defs, oct);
    assert(active.ok());
    assert(active.value().events.size() == 2);
    assert(active.value().events[0].id == EventId{4});
    assert(active.value().events[1].id == EventId{1});
    assert(!active.value().override_applied);

    CalendarState jan{};
    jan.date = CalendarDate{2027, 1, 1};
    const auto crossover = resolve_events(defs, jan);
    assert(crossover.ok());
    assert(crossover.value().size() == 1);
    assert(crossover.value()[0].id == EventId{2});

    CalendarState leap{};
    leap.date = CalendarDate{2028, 3, 1};
    const auto leap_active = resolve_events(defs, leap);
    assert(leap_active.ok());
    assert(leap_active.value().size() == 1);
    assert(leap_active.value()[0].id == EventId{3});

    CalendarState non_leap{};
    non_leap.date = CalendarDate{2027, 3, 1};
    const auto no_leap = resolve_events(defs, non_leap);
    assert(no_leap.ok());
    assert(no_leap.value().empty());

    // Forced selectors are contextual and require explicit authority provenance.
    CalendarState july{};
    const CalendarDate july4{2026, 7, 4};
    july.date = july4;

    EventSelector unauthorized{};
    unauthorized.include_calendar_events = false;
    unauthorized.forced_event = EventId{1};
    const auto unauthorized_result = resolve_event_set(defs, july, unauthorized);
    assert(!unauthorized_result.ok());
    assert(unauthorized_result.error().code == ErrorCode::ValidationFailed);
    assert(july.date == july4);

    EventSelector forced{};
    forced.include_calendar_events = false;
    forced.forced_event = EventId{1};
    forced.authority = "selector.cheat.halloween";
    const auto override_result = resolve_event_set(defs, july, forced);
    assert(override_result.ok());
    assert(override_result.value().events.size() == 1);
    assert(override_result.value().events[0].id == EventId{1});
    assert(override_result.value().override_applied);
    assert(override_result.value().forced_event == EventId{1});
    assert(override_result.value().authority == "selector.cheat.halloween");
    assert(july.date == july4);

    EventSelector missing{};
    missing.forced_event = EventId{99};
    missing.authority = "selector.test";
    const auto missing_result = resolve_event_set(defs, july, missing);
    assert(!missing_result.ok());
    assert(missing_result.error().code == ErrorCode::NotFound);

    // Definitions reject malformed dates, duplicate affinity tags, ids, and keys.
    EventDefinition invalid_date = halloween;
    invalid_date.id = EventId{50};
    invalid_date.key = "bad_date";
    invalid_date.rule = AnnualDateRule{2, 30, 1};
    assert(!validate_event_definition(invalid_date).ok());

    EventDefinition duplicate_affinity = halloween;
    duplicate_affinity.id = EventId{51};
    duplicate_affinity.key = "dupe_affinity";
    duplicate_affinity.affinities = {"same", "same"};
    const auto duplicate_affinity_result = validate_event_definition(duplicate_affinity);
    assert(!duplicate_affinity_result.ok());
    assert(duplicate_affinity_result.error().code == ErrorCode::AlreadyExists);

    EventCatalog catalog;
    assert(catalog.add(year_turn).ok());
    assert(catalog.add(halloween).ok());
    assert(catalog.add(leap_watch).ok());
    assert(catalog.size() == 3);

    EventDefinition duplicate_id = harvest;
    duplicate_id.id = EventId{1};
    const auto duplicate_id_result = catalog.add(duplicate_id);
    assert(!duplicate_id_result.ok());
    assert(duplicate_id_result.error().code == ErrorCode::AlreadyExists);

    EventDefinition duplicate_key = harvest;
    duplicate_key.id = EventId{40};
    duplicate_key.key = "halloween";
    const auto duplicate_key_result = catalog.add(duplicate_key);
    assert(!duplicate_key_result.ok());
    assert(duplicate_key_result.error().code == ErrorCode::AlreadyExists);

    const auto deterministic = catalog.snapshot();
    assert(deterministic.size() == 3);
    assert(deterministic[0].id == EventId{1});
    assert(deterministic[1].id == EventId{2});
    assert(deterministic[2].id == EventId{3});

    // HOME owns event definitions without letting catalog/override operations rewrite chronology.
    VersionedWorld world{WorldId{10}, WorldClockConfig{}, CalendarConfig{july4}};
    const WorldTime canonical_before = world.clock().now();
    const WorldRevision revision_before = world.revision();
    assert(world.add_event_definition(halloween).ok());
    assert(world.add_event_definition(year_turn).ok());
    assert(world.events().size() == 2);
    assert(world.clock().now() == canonical_before);
    assert(world.revision() == revision_before);

    const auto natural_world_events = world.active_events();
    assert(natural_world_events.ok());
    assert(natural_world_events.value().events.empty());
    assert(world.clock().now() == canonical_before);
    assert(world.revision() == revision_before);

    const auto forced_world_events = world.active_events(forced);
    assert(forced_world_events.ok());
    assert(forced_world_events.value().events.size() == 1);
    assert(forced_world_events.value().events[0].id == EventId{1});
    assert(world.calendar().date == july4);
    assert(world.clock().now() == canonical_before);
    assert(world.revision() == revision_before);

    // Event definitions persist, but contextual forced selectors do not.
    const WorldSnapshot captured = world.snapshot();
    assert(captured.events.size() == 2);
    const auto encoded = encode_snapshot(captured);
    assert(encoded.ok());
    assert(encoded.value().rfind("HOME_SNAPSHOT ", 0) == 0);
    assert(encoded.value().find("selector.cheat.halloween") == std::string::npos);

    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().events.size() == 2);
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    const VersionedWorld& restored = restored_result.value();
    assert(restored.events().size() == 2);
    assert(restored.events().find(EventId{1}) != nullptr);
    assert(restored.events().find(EventId{1})->affinities.size() == 2);
    assert(restored.active_events().ok());
    assert(restored.active_events().value().events.empty());
    assert(restored.clock().now() == canonical_before);
    assert(restored.revision() == revision_before);

    // v3 saves remain valid and simply contain no persisted event catalog.
    const auto legacy_v3 = decode_snapshot(
        "HOME_SNAPSHOT 3\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "ENTITIES 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(legacy_v3.ok());
    assert(legacy_v3.value().events.empty());

    WorldSnapshot malformed = captured;
    malformed.events.push_back(halloween);
    const auto malformed_encoded = encode_snapshot(malformed);
    assert(!malformed_encoded.ok());
    assert(malformed_encoded.error().code == ErrorCode::AlreadyExists);

    assert(std::string_view{restored.events().find(EventId{1})->display_name} == "Halloween");
    return 0;
}
