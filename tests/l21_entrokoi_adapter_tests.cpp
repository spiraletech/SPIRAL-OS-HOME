#include "home/entrokoi_adapter.hpp"
#include "home/snapshot.hpp"

#include <algorithm>
#include <cassert>

namespace {

template <typename T>
concept HasApply = requires(T adapter) {
    adapter.apply();
};

home::PlayerLifeState life_for(home::EntityId entity) {
    home::PlayerLifeState state{};
    state.entity = entity;
    state.stage = home::LifeStage::Adult;
    state.presence = home::LifePresence::Present;
    state.born_world_minute = 0;
    state.updated_world_minute = 0;
    state.sequence = 1;
    return state;
}

home::PlayerDynamicsState dynamics_for(home::EntityId entity, std::int32_t valence, std::int32_t arousal) {
    home::PlayerDynamicsState state{};
    state.entity = entity;
    state.mood.valence = valence;
    state.mood.arousal = arousal;
    state.mood.band = home::derive_mood_band(valence);
    state.autonomy.mode = home::AutonomyMode::Disabled;
    state.updated_world_minute = 0;
    state.sequence = 1;
    return state;
}

const home::EntrokoiEntityState* find_entity(
    const home::EntrokoiWorldView& view,
    home::EntityId entity) {
    const auto it = std::find_if(view.entities.begin(), view.entities.end(), [&](const auto& state) {
        return state.entity == entity;
    });
    return it == view.entities.end() ? nullptr : &*it;
}

} // namespace

static_assert(!HasApply<home::EntrokoiAdapter>);

int main() {
    using namespace home;

    VersionedWorld world{
        WorldId{21},
        WorldClockConfig{},
        CalendarConfig{CalendarDate{2026, 10, 31}}};

    ZoneCreateInfo district_info{};
    district_info.kind = ZoneKind::District;
    district_info.key = "mission-bay";
    district_info.display_name = "Mission Bay";
    const auto district_result = world.create_zone(district_info);
    assert(district_result);
    const ZoneId district = district_result.value();

    ZoneCreateInfo room_info{};
    room_info.kind = ZoneKind::Room;
    room_info.key = "studio";
    room_info.display_name = "Studio";
    room_info.parent = district;
    const auto room_result = world.create_zone(room_info);
    assert(room_result);
    const ZoneId room = room_result.value();

    ZoneConnection connection{};
    connection.from = district;
    connection.to = room;
    connection.bidirectional = true;
    connection.traversable = true;
    connection.tag = "door";
    assert(world.connect_zones(connection));

    ClimateProfile climate{};
    climate.zone = room;
    climate.mean_temperature_millicelsius = 16'000;
    climate.wetness_permille = 900;
    climate.wind_permille = 500;
    climate.seed = 21;
    assert(world.set_climate_profile(climate));
    assert(world.evolve_zone_weather(room, 77));

    EventDefinition halloween{};
    halloween.id = EventId{1};
    halloween.key = "halloween";
    halloween.display_name = "Halloween";
    halloween.kind = EventKind::Holiday;
    halloween.rule = AnnualDateRule{10, 31, 1};
    halloween.priority = 100;
    halloween.affinities = {"calendar.halloween"};
    assert(world.add_event_definition(halloween));
    assert(world.refresh_world_context(room));

    EntityCreateInfo observer_info{};
    observer_info.kind = EntityKind::Avatar;
    observer_info.archetype = "agnathos";
    observer_info.display_name = "Agnathos";
    observer_info.transform.position = Vec3Mm{1000, 2000, 3000};
    const auto observer_result = world.create_entity(observer_info);
    assert(observer_result);
    const EntityId observer = observer_result.value();

    EntityCreateInfo target_info{};
    target_info.kind = EntityKind::Avatar;
    target_info.archetype = "friend";
    target_info.display_name = "Friend";
    target_info.transform.position = Vec3Mm{4000, 2000, 3000};
    const auto target_result = world.create_entity(target_info);
    assert(target_result);
    const EntityId target = target_result.value();

    assert(world.place_entity(observer, room));
    assert(world.place_entity(target, district));
    assert(world.set_player_life_state(life_for(observer)));
    assert(world.set_player_life_state(life_for(target)));
    assert(world.set_player_dynamics_state(dynamics_for(observer, 5'000, 8'000)));
    assert(world.set_player_dynamics_state(dynamics_for(target, -4'000, 2'000)));

    RelationshipState observer_to_target{};
    observer_to_target.from = observer;
    observer_to_target.to = target;
    observer_to_target.kind = RelationshipKind::Friend;
    observer_to_target.affinity = 7'500;
    observer_to_target.trust = 8'000;
    observer_to_target.sequence = 1;
    assert(world.set_relationship_state(observer_to_target));

    RelationshipState target_to_observer{};
    target_to_observer.from = target;
    target_to_observer.to = observer;
    target_to_observer.kind = RelationshipKind::Rival;
    target_to_observer.affinity = -3'000;
    target_to_observer.trust = 2'000;
    target_to_observer.sequence = 1;
    assert(world.set_relationship_state(target_to_observer));

    SubclassAffinityState subclass{};
    subclass.player = observer;
    subclass.key = "explorer";
    subclass.evidence_points = 600;
    subclass.affinity_permille = subclass_affinity_for_evidence(subclass.evidence_points);
    subclass.discovered_world_minute = 0;
    subclass.updated_world_minute = 0;
    subclass.sequence = 1;
    assert(world.set_subclass_affinity(subclass));

    ItemCreateInfo item_info{};
    item_info.archetype_key = "camera";
    item_info.display_name = "Camera";
    item_info.kind = ItemKind::Equipment;
    item_info.quantity = 1;
    item_info.max_stack = 1;
    item_info.durability = 8'500;
    item_info.zone = room;
    const auto item_result = world.create_item(item_info);
    assert(item_result);

    EntrokoiAdapter adapter{world};
    const WorldRevision before_revision = world.revision();
    const std::size_t before_history = world.history().size();

    const auto first_result = adapter.snapshot_for(observer);
    assert(first_result);
    const EntrokoiWorldView first = first_result.value();
    const auto second_result = adapter.snapshot_for(observer);
    assert(second_result);
    assert(second_result.value() == first);
    assert(world.revision() == before_revision);
    assert(world.history().size() == before_history);

    assert(first.projection_version == kEntrokoiProjectionVersion);
    assert(first.world == WorldId{21});
    assert(first.revision == before_revision);
    assert(first.world_time == world.clock().now());
    assert(first.calendar == world.calendar());
    assert(first.affect == world.affect());
    assert(first.anchor == world.anchor());
    assert(first.observer.entity == observer);
    assert(first.observer.zone == room);
    assert(first.observer.mood.has_value());
    assert(first.observer.mood->band == MoodBand::Positive);
    assert(first.observer.aura.has_value());
    assert(first.observer.aura->dominant_affinity == "explorer");
    assert(!first.observer.relation.has_value());

    assert(first.active_events.size() == 1);
    assert(first.active_events.front().key == "halloween");
    assert(first.connections.size() == 1);
    assert(first.zones.size() == 2);
    const auto room_it = std::find_if(first.zones.begin(), first.zones.end(), [&](const auto& state) {
        return state.zone == room;
    });
    assert(room_it != first.zones.end());
    assert(room_it->climate.has_value());
    assert(room_it->weather.has_value());

    const EntrokoiEntityState* target_state = find_entity(first, target);
    assert(target_state != nullptr);
    assert(target_state->relation.has_value());
    assert(target_state->relation->kind == RelationshipKind::Friend);
    assert(target_state->mood.has_value());
    assert(target_state->mood->band == MoodBand::Low);

    assert(first.items.size() == 1);
    assert(first.items.front().item == item_result.value());
    assert(first.items.front().zone == room);
    assert(first.items.front().durability == 8'500);

    // The same canonical world projects a different directed social fact for another observer.
    const auto reverse_result = adapter.snapshot_for(target);
    assert(reverse_result);
    const EntrokoiEntityState* observer_as_seen_from_target = find_entity(reverse_result.value(), observer);
    assert(observer_as_seen_from_target != nullptr);
    assert(observer_as_seen_from_target->relation.has_value());
    assert(observer_as_seen_from_target->relation->kind == RelationshipKind::Rival);

    // Unknown observers fail without mutating HOME.
    const WorldRevision before_unknown = world.revision();
    const auto unknown = adapter.snapshot_for(EntityId{999999});
    assert(!unknown);
    assert(unknown.error().code == ErrorCode::NotFound);
    assert(world.revision() == before_unknown);

    // Frames are immutable value snapshots; later HOME truth produces a new frame only.
    Transform moved = observer_info.transform;
    moved.position = Vec3Mm{9000, 2000, 3000};
    assert(world.update_transform(observer, moved));
    const auto moved_result = adapter.snapshot_for(observer);
    assert(moved_result);
    assert(moved_result.value().observer.transform == moved);
    assert(first.observer.transform == observer_info.transform);

    // L21 is runtime projection only; persistent HOME format remains v12.
    const auto encoded = encode_snapshot(world.snapshot());
    assert(encoded);
    assert(encoded.value().rfind("HOME_SNAPSHOT 12", 0) == 0);

    return 0;
}
