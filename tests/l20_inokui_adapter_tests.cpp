#include "home/inokui_adapter.hpp"

#include <algorithm>
#include <cassert>
#include <string>

template <typename T>
concept HasApplyMember = requires { &T::apply; };

namespace {

home::PlayerLifeState life_for(home::EntityId entity) {
    home::PlayerLifeState state{};
    state.entity = entity;
    state.stage = home::LifeStage::Adult;
    state.presence = home::LifePresence::Present;
    state.born_world_minute = 0;
    state.updated_world_minute = 1;
    state.sequence = 1;
    return state;
}

const home::InokuiZoneState* find_zone(const home::InokuiWorldView& view, home::ZoneId id) {
    const auto it = std::find_if(view.zones.begin(), view.zones.end(), [&](const auto& zone) { return zone.zone == id; });
    return it == view.zones.end() ? nullptr : &*it;
}

const home::InokuiEntityState* find_entity(const home::InokuiWorldView& view, home::EntityId id) {
    const auto it = std::find_if(view.entities.begin(), view.entities.end(), [&](const auto& entity) { return entity.entity == id; });
    return it == view.entities.end() ? nullptr : &*it;
}

const home::InokuiItemState* find_item(const home::InokuiWorldView& view, home::ItemId id) {
    const auto it = std::find_if(view.items.begin(), view.items.end(), [&](const auto& item) { return item.item == id; });
    return it == view.items.end() ? nullptr : &*it;
}

} // namespace

int main() {
    using namespace home;

    static_assert(!HasApplyMember<InokuiAdapter>);

    VersionedWorld world{WorldId{20}};
    assert(world.advance_time(30'000).ok()); // HOME minute 1.

    ZoneCreateInfo bay_info{};
    bay_info.kind = ZoneKind::District;
    bay_info.key = "crown_point";
    bay_info.display_name = "Crown Point";
    const auto bay = world.create_zone(bay_info);
    assert(bay.ok());

    ZoneCreateInfo interior_info{};
    interior_info.kind = ZoneKind::Interior;
    interior_info.key = "dry_room";
    interior_info.display_name = "Dry Room";
    interior_info.parent = bay.value();
    const auto dry_room = world.create_zone(interior_info);
    assert(dry_room.ok());

    ClimateProfile climate{};
    climate.zone = bay.value();
    climate.mean_temperature_millicelsius = 19000;
    climate.wetness_permille = 650;
    climate.wind_permille = 350;
    climate.seed = 2026;
    assert(world.set_climate_profile(climate).ok());
    const auto evolved = world.evolve_zone_weather(bay.value(), 0xC0FFEEULL);
    assert(evolved.ok());

    EventDefinition new_year{};
    new_year.id = EventId{1};
    new_year.key = "new_year";
    new_year.display_name = "New Year";
    new_year.kind = EventKind::Festival;
    new_year.rule = AnnualDateRule{1, 1, 1};
    new_year.priority = 50;
    new_year.affinities = {"calendar.new_year"};
    assert(world.add_event_definition(new_year).ok());

    EventDefinition inactive{};
    inactive.id = EventId{2};
    inactive.key = "winter_future";
    inactive.display_name = "Winter Future";
    inactive.kind = EventKind::Festival;
    inactive.rule = AnnualDateRule{12, 25, 1};
    inactive.priority = 100;
    assert(world.add_event_definition(inactive).ok());

    assert(world.refresh_world_context(bay.value()).ok());

    EntityCreateInfo avatar_info{};
    avatar_info.kind = EntityKind::Avatar;
    avatar_info.archetype = "agnathos";
    avatar_info.display_name = "Agnathos";
    avatar_info.transform.position = Vec3Mm{1000, 2000, 3000};
    const auto avatar = world.create_entity(avatar_info);
    assert(avatar.ok());
    assert(world.place_entity(avatar.value(), bay.value()).ok());
    assert(world.set_player_life_state(life_for(avatar.value())).ok());

    PlayerDynamicsState dynamics{};
    dynamics.entity = avatar.value();
    dynamics.mood.valence = 3'000;
    dynamics.mood.arousal = 4'000;
    dynamics.mood.band = MoodBand::Positive;
    dynamics.updated_world_minute = 1;
    dynamics.sequence = 1;
    assert(world.set_player_dynamics_state(dynamics).ok());

    SubclassAffinityState subclass{};
    subclass.player = avatar.value();
    subclass.key = "street_skater";
    subclass.evidence_points = 700;
    subclass.affinity_permille = 700;
    subclass.discovered_world_minute = 1;
    subclass.updated_world_minute = 1;
    subclass.sequence = 1;
    assert(world.set_subclass_affinity(subclass).ok());
    assert(world.theorism().find_aura(avatar.value()) != nullptr);

    EntityCreateInfo prop_info{};
    prop_info.kind = EntityKind::Environment;
    prop_info.archetype = "pier_lamp";
    prop_info.display_name = "Pier Lamp";
    const auto prop = world.create_entity(prop_info);
    assert(prop.ok());
    assert(world.place_entity(prop.value(), dry_room.value()).ok());

    ItemCreateInfo zone_item{};
    zone_item.archetype_key = "board_0042";
    zone_item.display_name = "Skateboard";
    zone_item.kind = ItemKind::Equipment;
    zone_item.quantity = 1;
    zone_item.max_stack = 1;
    zone_item.durability = 9'500;
    zone_item.zone = bay.value();
    zone_item.updated_world_minute = 1;
    const auto board = world.create_item(zone_item);
    assert(board.ok());

    ItemCreateInfo owned_item{};
    owned_item.archetype_key = "keycard";
    owned_item.display_name = "Keycard";
    owned_item.kind = ItemKind::Key;
    owned_item.owner = avatar.value();
    owned_item.updated_world_minute = 1;
    const auto keycard = world.create_item(owned_item);
    assert(keycard.ok());

    InokuiAdapter adapter{world};
    const WorldRevision before_projection = world.revision();
    const std::size_t history_before_projection = world.history().size();
    const auto projected_result = adapter.snapshot();
    assert(projected_result.ok());
    const InokuiWorldView projected = projected_result.value();

    // Projection is read-only and revision-stamped.
    assert(world.revision() == before_projection);
    assert(world.history().size() == history_before_projection);
    assert(projected.projection_version == kInokuiProjectionVersion);
    assert(projected.world == world.world());
    assert(projected.revision == world.revision());
    assert(projected.world_time == world.clock().now());
    assert(projected.calendar == world.calendar());
    assert(projected.affect == world.affect());
    assert(projected.anchor == world.anchor());

    // Only naturally active events are projected; no forced selector exists at the boundary.
    assert(projected.active_events.size() == 1);
    assert(projected.active_events.front().id == EventId{1});
    assert(projected.active_events.front().key == "new_year");

    // Zone manifestation inputs contain semantic climate/weather facts, not shaders/material commands.
    const auto* bay_projection = find_zone(projected, bay.value());
    assert(bay_projection != nullptr);
    assert(bay_projection->climate.has_value());
    assert(bay_projection->weather.has_value());
    assert(bay_projection->climate.value() == *world.climates().find(bay.value()));
    assert(bay_projection->weather.value() == *world.weather().find(bay.value()));
    const auto* dry_projection = find_zone(projected, dry_room.value());
    assert(dry_projection != nullptr);
    assert(!dry_projection->climate.has_value());
    assert(!dry_projection->weather.has_value());

    // Avatar presentation inputs include semantic life, mood and aura while ordinary props do not invent them.
    const auto* avatar_projection = find_entity(projected, avatar.value());
    assert(avatar_projection != nullptr);
    assert(avatar_projection->zone == bay.value());
    assert(avatar_projection->life_stage == LifeStage::Adult);
    assert(avatar_projection->life_presence == LifePresence::Present);
    assert(avatar_projection->mood.has_value());
    assert(avatar_projection->mood->band == MoodBand::Positive);
    assert(avatar_projection->aura.has_value());
    assert(avatar_projection->aura.value() == *world.theorism().find_aura(avatar.value()));
    const auto* prop_projection = find_entity(projected, prop.value());
    assert(prop_projection != nullptr);
    assert(!prop_projection->life_stage.has_value());
    assert(!prop_projection->mood.has_value());
    assert(!prop_projection->aura.has_value());

    // Item presentation is semantic location/condition metadata only.
    const auto* board_projection = find_item(projected, board.value());
    assert(board_projection != nullptr);
    assert(board_projection->zone == bay.value());
    assert(!board_projection->owner.has_value());
    assert(board_projection->durability == 9'500);
    const auto* keycard_projection = find_item(projected, keycard.value());
    assert(keycard_projection != nullptr);
    assert(keycard_projection->owner == avatar.value());
    assert(!keycard_projection->zone.has_value());

    // Identical HOME truth produces an identical deterministic manifestation frame.
    const auto projected_again = adapter.snapshot();
    assert(projected_again.ok());
    assert(projected_again.value() == projected);

    // Canonical HOME mutations produce a new frame; old frames remain immutable value snapshots.
    PlayerDynamicsState changed = *world.player_dynamics().find(avatar.value());
    changed.mood.valence = -3'000;
    changed.mood.arousal = 7'000;
    changed.mood.band = MoodBand::Low;
    changed.sequence = 2;
    const WorldRevision before_mood_change = world.revision();
    assert(world.set_player_dynamics_state(changed).ok());
    assert(world.revision() == before_mood_change.next().value());

    const auto refreshed_result = adapter.snapshot();
    assert(refreshed_result.ok());
    const auto* refreshed_avatar = find_entity(refreshed_result.value(), avatar.value());
    assert(refreshed_avatar != nullptr);
    assert(refreshed_avatar->mood->band == MoodBand::Low);
    assert(refreshed_avatar->aura.has_value());
    assert(refreshed_avatar->aura->signature != avatar_projection->aura->signature);
    assert(avatar_projection->mood->band == MoodBand::Positive);

    // L20 transport is runtime-only; canonical persistence format remains v12.
    const auto encoded = encode_snapshot(world.snapshot());
    assert(encoded.ok());
    assert(encoded.value().rfind("HOME_SNAPSHOT 12", 0) == 0);

    return 0;
}
