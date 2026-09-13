#include "home/inokui_adapter.hpp"

#include <algorithm>
#include <utility>

namespace home {

Result<InokuiWorldView> InokuiAdapter::snapshot() const {
    InokuiWorldView view{};
    view.world = world_.world();
    view.revision = world_.revision();
    view.world_time = world_.clock().now();
    view.calendar = world_.calendar();
    view.affect = world_.affect();
    view.anchor = world_.anchor();

    const auto events = world_.active_events();
    if (!events) {
        return Result<InokuiWorldView>::failure(events.error().code, events.error().message);
    }
    view.active_events.reserve(events.value().events.size());
    for (const auto& event : events.value().events) {
        view.active_events.push_back(InokuiEventState{
            event.id,
            event.key,
            event.display_name,
            event.kind,
            event.priority,
            event.affinities
        });
    }
    std::sort(view.active_events.begin(), view.active_events.end(), [](const InokuiEventState& a, const InokuiEventState& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.id.value() < b.id.value();
    });

    const auto zones = world_.topology().snapshot_zones();
    view.zones.reserve(zones.size());
    for (const auto& zone : zones) {
        InokuiZoneState projected{};
        projected.zone = zone.id;
        projected.kind = zone.kind;
        projected.key = zone.key;
        projected.display_name = zone.display_name;
        projected.parent = zone.parent;
        projected.persistent = zone.persistent;
        if (const auto* climate = world_.climates().find(zone.id)) projected.climate = *climate;
        if (const auto* weather = world_.weather().find(zone.id)) projected.weather = *weather;
        view.zones.push_back(std::move(projected));
    }
    std::sort(view.zones.begin(), view.zones.end(), [](const InokuiZoneState& a, const InokuiZoneState& b) {
        return a.zone.value() < b.zone.value();
    });

    const auto entities = world_.entities().snapshot();
    view.entities.reserve(entities.size());
    for (const auto& entity : entities) {
        InokuiEntityState projected{};
        projected.entity = entity.id;
        projected.kind = entity.kind;
        projected.archetype = entity.archetype;
        projected.display_name = entity.display_name;
        projected.transform = entity.transform;
        projected.zone = world_.topology().zone_of(entity.id);
        projected.persistent = entity.persistent;

        if (const auto* life = world_.player_life().find(entity.id)) {
            projected.life_stage = life->stage;
            projected.life_presence = life->presence;
        }
        if (const auto* dynamics = world_.player_dynamics().find(entity.id)) projected.mood = dynamics->mood;
        if (const auto* aura = world_.theorism().find_aura(entity.id)) projected.aura = *aura;

        view.entities.push_back(std::move(projected));
    }
    std::sort(view.entities.begin(), view.entities.end(), [](const InokuiEntityState& a, const InokuiEntityState& b) {
        return a.entity.value() < b.entity.value();
    });

    const auto items = world_.inventory().snapshot();
    view.items.reserve(items.size());
    for (const auto& item : items) {
        view.items.push_back(InokuiItemState{
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
    std::sort(view.items.begin(), view.items.end(), [](const InokuiItemState& a, const InokuiItemState& b) {
        return a.item.value() < b.item.value();
    });

    return Result<InokuiWorldView>::success(std::move(view));
}

} // namespace home
