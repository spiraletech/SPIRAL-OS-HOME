#include "home/xenon_adapter.hpp"

#include <algorithm>
#include <cassert>

using namespace home;

template <typename T>
concept HasApplyMember = requires { &T::apply; };

template <typename T>
concept HasSnapshotFor = requires(T adapter, EntityId listener) { adapter.snapshot_for(listener); };

namespace {

PlayerLifeState life_for(EntityId entity) {
    PlayerLifeState state{};
    state.entity = entity;
    state.stage = LifeStage::Adult;
    state.presence = LifePresence::Present;
    state.born_world_minute = 0;
    state.updated_world_minute = 1;
    state.sequence = 1;
    return state;
}

const XenonZoneState* find_zone(const XenonWorldView& view, ZoneId id) {
    const auto it = std::find_if(view.zones.begin(), view.zones.end(), [&](const auto& state) { return state.zone == id; });
    return it == view.zones.end() ? nullptr : &*it;
}

const XenonEntityState* find_entity(const XenonWorldView& view, EntityId id) {
    const auto it = std::find_if(view.entities.begin(), view.entities.end(), [&](const auto& state) { return state.entity == id; });
    return it == view.entities.end() ? nullptr : &*it;
}

const XenonItemState* find_item(const XenonWorldView& view, ItemId id) {
    const auto it = std::find_if(view.items.begin(), view.items.end(), [&](const auto& state) { return state.item == id; });
    return it == view.items.end() ? nullptr : &*it;
}

} // namespace

int main() {
    static_assert(!HasApplyMember<XenonAdapter>);
    static_assert(!HasSnapshotFor<XenonAdapter>);

    VersionedWorld world{WorldId{22}};
    assert(world.advance_time(30'000).ok());

    ZoneCreateInfo pier_info{};
    pier_info.kind = ZoneKind::District;
    pier_info.key = "crown_point";
    pier_info.display_name = "Crown Point";
    const auto pier = world.create_zone(pier_info);
    assert(pier.ok());

    ZoneCreateInfo tunnel_info{};
    tunnel_info.kind = ZoneKind::Interior;
    tunnel_info.key = "service_tunnel";
    tunnel_info.display_name = "Service Tunnel";
    tunnel_info.parent = pier.value();
    const auto tunnel = world.create_zone(tunnel_info);
    assert(tunnel.ok());

    ZoneConnection barrier{};
    barrier.from = pier.value();
    barrier.to = tunnel.value();
    barrier.bidirectional = true;
    barrier.traversable = false;
    barrier.tag = "maintenance_barrier";
    assert(world.connect_zones(barrier).ok());

    ClimateProfile climate{};
    climate.zone = pier.value();
    climate.mean_temperature_millicelsius = 18'500;
    climate.wetness_permille = 800;
    climate.wind_permille = 450;
    climate.seed = 22;
    assert(world.set_climate_profile(climate).ok());
    assert(world.evolve_zone_weather(pier.value(), 0x22ULL).ok());

    EventDefinition event{};
    event.id = EventId{1};
    event.key = "new_year";
    event.display_name = "New Year";
    event.kind = EventKind::Festival;
    event.rule = AnnualDateRule{1, 1, 1};
    event.priority = 50;
    event.affinities = {"calendar.new_year"};
    assert(world.add_event_definition(event).ok());

    EventDefinition inactive{};
    inactive.id = EventId{2};
    inactive.key = "future_festival";
    inactive.display_name = "Future Festival";
    inactive.kind = EventKind::Festival;
    inactive.rule = AnnualDateRule{12, 25, 1};
    inactive.priority = 100;
    assert(world.add_event_definition(inactive).ok());
    assert(world.refresh_world_context(pier.value()).ok());

    EntityCreateInfo avatar_info{};
    avatar_info.kind = EntityKind::Avatar;
    avatar_info.archetype = "agnathos";
    avatar_info.display_name = "Agnathos";
    avatar_info.transform.position = Vec3Mm{1000, 2000, 3000};
    const auto avatar = world.create_entity(avatar_info);
    assert(avatar.ok());
    assert(world.place_entity(avatar.value(), pier.value()).ok());
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
    subclass.key = "explorer";
    subclass.evidence_points = 650;
    subclass.affinity_permille = 650;
    subclass.discovered_world_minute = 1;
    subclass.updated_world_minute = 1;
    subclass.sequence = 1;
    assert(world.set_subclass_affinity(subclass).ok());

    EntityCreateInfo machine_info{};
    machine_info.kind = EntityKind::Environment;
    machine_info.archetype = "pump_station";
    machine_info.display_name = "Pump Station";
    machine_info.transform.position = Vec3Mm{5000, 0, 0};
    const auto machine = world.create_entity(machine_info);
    assert(machine.ok());
    assert(world.place_entity(machine.value(), tunnel.value()).ok());

    ItemCreateInfo board_info{};
    board_info.archetype_key = "board_0042";
    board_info.display_name = "Skateboard";
    board_info.kind = ItemKind::Equipment;
    board_info.quantity = 1;
    board_info.max_stack = 1;
    board_info.durability = 8'750;
    board_info.zone = pier.value();
    board_info.updated_world_minute = 1;
    const auto board = world.create_item(board_info);
    assert(board.ok());

    XenonAdapter adapter{world};
    const WorldRevision revision_before = world.revision();
    const std::size_t history_before = world.history().size();
    const auto projected_result = adapter.snapshot();
    assert(projected_result.ok());
    const XenonWorldView projected = projected_result.value();

    assert(world.revision() == revision_before);
    assert(world.history().size() == history_before);
    assert(projected.projection_version == kXenonProjectionVersion);
    assert(projected.world == world.world());
    assert(projected.revision == world.revision());
    assert(projected.world_time == world.clock().now());
    assert(projected.calendar == world.calendar());
    assert(projected.affect == world.affect());
    assert(projected.anchor == world.anchor());

    // XENON receives natural event identity as acoustic/signal context, not music commands.
    assert(projected.active_events.size() == 1);
    assert(projected.active_events.front().key == "new_year");

    // Climate/weather and topology are semantic acoustic causes; HOME does not derive DSP from them.
    const auto* pier_state = find_zone(projected, pier.value());
    assert(pier_state != nullptr);
    assert(pier_state->climate.has_value());
    assert(pier_state->weather.has_value());
    assert(pier_state->climate.value() == *world.climates().find(pier.value()));
    assert(pier_state->weather.value() == *world.weather().find(pier.value()));
    assert(projected.connections.size() == 1);
    assert(projected.connections.front() == barrier);

    // Entity projection gives XENON canonical source context without inventing emitters or clips.
    const auto* avatar_state = find_entity(projected, avatar.value());
    assert(avatar_state != nullptr);
    assert(avatar_state->zone == pier.value());
    assert(avatar_state->life_stage == LifeStage::Adult);
    assert(avatar_state->life_presence == LifePresence::Present);
    assert(avatar_state->mood.has_value());
    assert(avatar_state->mood->band == MoodBand::Positive);
    assert(avatar_state->aura.has_value());
    assert(avatar_state->aura.value() == *world.theorism().find_aura(avatar.value()));

    const auto* machine_state = find_entity(projected, machine.value());
    assert(machine_state != nullptr);
    assert(machine_state->zone == tunnel.value());
    assert(!machine_state->life_stage.has_value());
    assert(!machine_state->mood.has_value());
    assert(!machine_state->aura.has_value());

    const auto* board_state = find_item(projected, board.value());
    assert(board_state != nullptr);
    assert(board_state->zone == pier.value());
    assert(board_state->durability == 8'750);

    // Identical canonical truth produces an identical global acoustic/signal frame.
    const auto again = adapter.snapshot();
    assert(again.ok());
    assert(again.value() == projected);

    // HOME changes produce a new frame while old frames remain immutable values.
    PlayerDynamicsState changed = *world.player_dynamics().find(avatar.value());
    changed.mood.valence = -3'000;
    changed.mood.arousal = 7'000;
    changed.mood.band = MoodBand::Low;
    changed.sequence = 2;
    assert(world.set_player_dynamics_state(changed).ok());

    const auto refreshed = adapter.snapshot();
    assert(refreshed.ok());
    const auto* refreshed_avatar = find_entity(refreshed.value(), avatar.value());
    assert(refreshed_avatar != nullptr);
    assert(refreshed_avatar->mood->band == MoodBand::Low);
    assert(refreshed_avatar->aura.has_value());
    assert(refreshed_avatar->aura->signature != avatar_state->aura->signature);
    assert(avatar_state->mood->band == MoodBand::Positive);

    // XENON transport remains runtime-only; HOME persistence is still v12.
    const auto encoded = encode_snapshot(world.snapshot());
    assert(encoded.ok());
    assert(encoded.value().rfind("HOME_SNAPSHOT 12", 0) == 0);

    return 0;
}
