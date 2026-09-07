#include "home/versioned_world.hpp"

#include <algorithm>
#include <utility>

namespace home {

WorldSnapshot VersionedWorld::snapshot() const {
    WorldSnapshot out{};
    out.world = world();
    out.revision = revision_;
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
    if (!input.world.valid()) {
        return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot world id is invalid");
    }

    VersionedWorld restored{input.world};

    // Restore zone identities first without parents so hydration is independent of ID ordering.
    for (const auto& source : input.zones) {
        ZoneRecord zone = source;
        if (zone.world != input.world) {
            return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot contains a foreign zone");
        }
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

    for (const auto& entity : input.entities) {
        if (entity.world != input.world) {
            return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot contains a foreign entity");
        }
        const auto result = restored.registry_.restore(entity);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    for (const auto& connection : input.connections) {
        const auto result = restored.topology_.connect(connection);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    for (const auto& placement : input.placements) {
        if (!restored.registry_.contains(placement.entity)) {
            return Result<VersionedWorld>::failure(ErrorCode::ValidationFailed, "snapshot placement references a missing entity");
        }
        const auto result = restored.topology_.place_entity(placement.entity, placement.zone);
        if (!result) return Result<VersionedWorld>::failure(result.error().code, result.error().message);
    }

    restored.revision_ = input.revision;
    restored.history_.clear();
    return Result<VersionedWorld>::success(std::move(restored));
}

} // namespace home
