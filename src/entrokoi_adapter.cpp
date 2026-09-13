#include "home/entrokoi_adapter.hpp"

namespace home {
namespace {

EntrokoiEntityState project_entity(
    const VersionedWorld& world,
    const EntityRecord& record,
    EntityId observer) {
    EntrokoiEntityState state{};
    state.entity = record.id;
    state.kind = record.kind;
    state.archetype = record.archetype;
    state.display_name = record.display_name;
    state.transform = record.transform;
    state.zone = world.topology().zone_of(record.id);
    state.persistent = record.persistent;

    if (const auto* life = world.player_life().find(record.id)) {
        state.life_stage = life->stage;
        state.life_presence = life->presence;
    }
    if (const auto* dynamics = world.player_dynamics().find(record.id)) {
        state.mood = dynamics->mood;
    }
    if (const auto* aura = world.theorism().find_aura(record.id)) {
        state.aura = *aura;
    }
    if (record.id != observer) {
        if (const auto* relation = world.relationships().find_relationship(observer, record.id)) {
            state.relation = *relation;
        }
    }
    return state;
}

} // namespace

Result<EntrokoiWorldView> EntrokoiAdapter::snapshot_for(EntityId observer) const {
    const EntityRecord* observer_record = world_.entities().find(observer);
    if (observer_record == nullptr) {
        return Result<EntrokoiWorldView>::failure(
            ErrorCode::NotFound,
            "ENTROKOI observer references an unknown HOME entity");
    }

    const auto resolved_events = world_.active_events();
    if (!resolved_events) {
        return Result<EntrokoiWorldView>::failure(
            resolved_events.error().code,
            resolved_events.error().message);
    }

    EntrokoiWorldView view{};
    view.world = world_.world();
    view.revision = world_.revision();
    view.world_time = world_.clock().now();
    view.calendar = world_.calendar();
    view.affect = world_.affect();
    view.anchor = world_.anchor();
    view.observer = project_entity(world_, *observer_record, observer);

    view.active_events.reserve(resolved_events.value().events.size());
    for (const auto& event : resolved_events.value().events) {
        view.active_events.push_back(EntrokoiEventState{
            event.id,
            event.key,
            event.display_name,
            event.kind,
            event.priority,
            event.affinities
        });
    }

    const auto zones = world_.topology().snapshot_zones();
    view.zones.reserve(zones.size());
    for (const auto& zone : zones) {
        EntrokoiZoneState state{};
        state.zone = zone.id;
        state.kind = zone.kind;
        state.key = zone.key;
        state.display_name = zone.display_name;
        state.parent = zone.parent;
        state.persistent = zone.persistent;
        if (const auto* climate = world_.climates().find(zone.id)) {
            state.climate = *climate;
        }
        if (const auto* weather = world_.weather().find(zone.id)) {
            state.weather = *weather;
        }
        view.zones.push_back(std::move(state));
    }
    view.connections = world_.topology().snapshot_connections();

    const auto entities = world_.entities().snapshot();
    view.entities.reserve(entities.size());
    for (const auto& entity : entities) {
        view.entities.push_back(project_entity(world_, entity, observer));
    }

    const auto items = world_.inventory().snapshot();
    view.items.reserve(items.size());
    for (const auto& item : items) {
        view.items.push_back(EntrokoiItemState{
            item.id,
            item.archetype_key,
            item.display_name,
            item.kind,
            item.quantity,
            item.max_stack,
            item.durability,
            item.owner,
            item.zone
        });
    }

    return Result<EntrokoiWorldView>::success(std::move(view));
}

} // namespace home
