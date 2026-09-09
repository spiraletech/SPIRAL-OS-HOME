#include "home/versioned_world.hpp"

#include <algorithm>
#include <utility>

namespace home {

WorldSnapshot VersionedWorld::snapshot() const {
    WorldSnapshot out{};
    out.world = world();
    out.revision = revision_;
    out.clock_config = clock_.config();
    out.world_time = clock_.now();
    out.clock_remainder = clock_.remainder();
    out.calendar_config = calendar_config_;
    out.events = events_.snapshot();
    out.climates = climates_.snapshot();
    out.weather = weather_.snapshot();
    out.affect = affect_;
    out.anchor = anchor_;
    out.player_life = player_life_.snapshot();
    out.player_dynamics = player_dynamics_.snapshot();
    out.relationships = relationships_.relationship_snapshot();
    out.households = relationships_.household_snapshot();
    out.items = inventory_.snapshot();
    out.skills = progression_.skill_snapshot();
    out.tasks = progression_.task_snapshot();
    out.quests = progression_.quest_snapshot();
    out.entities = registry_.snapshot();
    out.zones = topology_.snapshot_zones();
    out.connections = topology_.snapshot_connections();
    for (const auto& entity : out.entities) {
        const auto zone = topology_.zone_of(entity.id);
        if (zone.has_value()) out.placements.push_back(SnapshotPlacement{entity.id, *zone});
    }
    std::sort(out.placements.begin(), out.placements.end(), [](const SnapshotPlacement& a, const SnapshotPlacement& b) {
        return a.entity.value() < b.entity.value();
    });
    return out;
}

Result<VersionedWorld> VersionedWorld::from_snapshot(const WorldSnapshot& input) {
    if (!input.world.valid()) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot world id is invalid");
    if (input.clock_config.real_milliseconds_per_home_minute == 0) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot clock configuration is invalid");
    if (!valid_calendar_date(input.calendar_config.epoch)) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot calendar configuration is invalid");
    const auto affect_valid = validate_world_affect_state(input.affect);
    if (!affect_valid) return Result<VersionedWorld>::failure(affect_valid.error().code, affect_valid.error().message);
    const auto anchor_valid = validate_world_anchor_state(input.anchor);
    if (!anchor_valid) return Result<VersionedWorld>::failure(anchor_valid.error().code, anchor_valid.error().message);

    VersionedWorld restored{input.world, input.clock_config, input.calendar_config};
    const auto clock_restore = restored.clock_.restore(input.world_time, input.clock_remainder);
    if (!clock_restore) return Result<VersionedWorld>::failure(clock_restore.error().code, clock_restore.error().message);

    for (const auto& event : input.events) {
        const auto result = restored.events_.add(event);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    for (const auto& source : input.zones) {
        ZoneRecord zone = source;
        if (zone.world != input.world) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot contains a foreign zone");
        zone.parent.reset();
        const auto result = restored.topology_.restore_zone(std::move(zone));
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& zone : input.zones) {
        if (zone.parent.has_value()) {
            const auto result = restored.topology_.set_parent(zone.id, zone.parent);
            if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
        }
    }

    for (const auto& climate : input.climates) {
        if (!restored.topology_.contains(climate.zone)) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot climate references a missing zone");
        if (restored.climates_.find(climate.zone)) return Result<VersionedWorld>::failure(ErrorCode::AlreadyExists, "snapshot contains duplicate climate zone");
        const auto result = restored.climates_.set(climate);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& state : input.weather) {
        if (!restored.topology_.contains(state.zone) || !restored.climates_.find(state.zone)) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot weather references a missing zone or climate");
        if (restored.weather_.find(state.zone)) return Result<VersionedWorld>::failure(ErrorCode::AlreadyExists, "snapshot contains duplicate weather zone");
        const auto result = restored.weather_.set(state);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    for (const auto& entity : input.entities) {
        if (entity.world != input.world) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot contains a foreign entity");
        const auto result = restored.registry_.restore(entity);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& connection : input.connections) {
        const auto result = restored.topology_.connect(connection);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& placement : input.placements) {
        if (!restored.registry_.contains(placement.entity)) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot placement references a missing entity");
        const auto result = restored.topology_.place_entity(placement.entity, placement.zone);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    const std::uint64_t current_world_minute = input.world_time.milliseconds / 60000ULL;
    for (const auto& life : input.player_life) {
        if (life.born_world_minute > current_world_minute || life.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot player life references a future HOME minute");
        if (restored.player_life_.find(life.entity)) return Result<VersionedWorld>::failure(ErrorCode::AlreadyExists, "snapshot contains duplicate player life entity");
        const auto result = restored.player_life_.set(restored.registry_, restored.topology_, life);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    for (const auto& dynamics : input.player_dynamics) {
        if (dynamics.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot player dynamics references a future HOME minute");
        if (restored.player_dynamics_.find(dynamics.entity)) return Result<VersionedWorld>::failure(ErrorCode::AlreadyExists, "snapshot contains duplicate player dynamics entity");
        const auto result = restored.player_dynamics_.set(restored.registry_, restored.player_life_, dynamics);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    for (const auto& relationship : input.relationships) {
        if (relationship.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot relationship references a future HOME minute");
        const auto result = restored.relationships_.restore_relationship(restored.registry_, restored.player_life_, relationship);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& household : input.households) {
        if (household.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot household references a future HOME minute");
        const auto result = restored.relationships_.restore_household(restored.registry_, restored.topology_, restored.player_life_, household);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& item : input.items) {
        if (item.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot item references a future HOME minute");
        const auto result = restored.inventory_.restore_item(restored.registry_, restored.topology_, restored.player_life_, item);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& skill : input.skills) {
        if (skill.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot skill references a future HOME minute");
        const auto result = restored.progression_.restore_skill(restored.registry_, restored.player_life_, skill);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& task : input.tasks) {
        if (task.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot task references a future HOME minute");
        const auto result = restored.progression_.restore_task(restored.registry_, restored.player_life_, task);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }
    for (const auto& quest : input.quests) {
        if (quest.updated_world_minute > current_world_minute) return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot quest references a future HOME minute");
        const auto result = restored.progression_.restore_quest(restored.registry_, restored.player_life_, quest);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    restored.affect_ = input.affect;
    restored.anchor_ = input.anchor;
    restored.revision_ = input.revision;
    restored.history_.clear();
    return Result<VersionedWorld>::success(std::move(restored));
}

} // namespace home
