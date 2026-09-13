#include "home/xenon_adapter.hpp"

#include <algorithm>
#include <utility>

namespace home {

Result<XenonWorldView> XenonAdapter::snapshot() const {
    XenonWorldView view{};
    view.world = world_.world();
    view.revision = world_.revision();
    view.world_time = world_.clock().now();
    view.calendar = world_.calendar();
    view.affect = world_.affect();
    view.anchor = world_.anchor();

    const auto events = world_.active_events();
    if (!events) {
        return Result<XenonWorldView>::failure(events.error().code, events.error().message);
    }
    view.active_events.reserve(events.value().events.size());
    for (const auto& event : events.value().events) {
        view.active_events.push_back(XenonEventState{
            event.id,
            event.key,
            event.display_name,
            event.kind,
            event.priority,
            event.affinities
        });
    }
    std::sort(view.active_events.begin(), view.active_events.end(), [](const XenonEventState& a, const XenonEventState& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.id.value() < b.id.value();
    });

    const auto zones = world_.topology().snapshot_zones();
    view.zones.reserve(zones.size());
    for (const auto& zone : zones) {
        XenonZoneState projected{};
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
    std::sort(view.zones.begin(), view.zones.end(), [](const XenonZoneState& a, const XenonZoneState& b) {
        return a.zone.value() < b.zone.value();
    });

    view.connections = world_.topology().snapshot_connections();
    std::sort(view.connections.begin(), view.connections.end(), [](const ZoneConnection& a, const ZoneConnection& b) {
        if (a.from != b.from) return a.from.value() < b.from.value();
        if (a.to != b.to) return a.to.value() < b.to.value();
        if (a.bidirectional != b.bidirectional) return a.bidirectional < b.bidirectional;
        if (a.traversable != b.traversable) return a.traversable < b.traversable;
        return a.tag < b.tag;
    });

    const auto entities = world_.entities().snapshot();
    view.entities.reserve(entities.size());
    for (const auto& entity : entities) {
        XenonEntityState projected{};
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
    std::sort(view.entities.begin(), view.entities.end(), [](const XenonEntityState& a, const XenonEntityState& b) {
        return a.entity.value() < b.entity.value();
    });

    const auto items = world_.inventory().snapshot();
    view.items.reserve(items.size());
    for (const auto& item : items) {
        view.items.push_back(XenonItemState{
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
    std::sort(view.items.begin(), view.items.end(), [](const XenonItemState& a, const XenonItemState& b) {
        return a.item.value() < b.item.value();
    });

    return Result<XenonWorldView>::success(std::move(view));
}

} // namespace home
