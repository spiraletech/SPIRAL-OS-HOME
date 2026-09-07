#include "home/versioned_world.hpp"

#include <utility>

namespace home {

VersionedWorld::VersionedWorld(WorldId world) noexcept : registry_(world), topology_(world) {}

Result<void> VersionedWorld::commit(std::vector<WorldChange> changes) {
    if (changes.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "cannot commit an empty world delta");
    }

    const auto next = revision_.next();
    if (!next.has_value()) {
        return Result<void>::failure(ErrorCode::Overflow, "world revision space exhausted");
    }

    const WorldRevision from = revision_;
    revision_ = *next;
    history_.emplace_back(from, revision_, std::move(changes));
    return Result<void>::success();
}

Result<EntityId> VersionedWorld::create_entity(EntityCreateInfo info) {
    const auto created = registry_.create(std::move(info));
    if (!created) return created;

    const EntityId id = created.value();
    const EntityRecord* record = registry_.find(id);
    if (record == nullptr) {
        return Result<EntityId>::failure(ErrorCode::InternalError, "created entity missing from registry");
    }

    const auto committed = commit({WorldChange{WorldChangeKind::EntityCreated, EntityCreated{*record}}});
    if (!committed) {
        registry_.remove(id);
        return Result<EntityId>::failure(committed.error().code, committed.error().message);
    }
    return Result<EntityId>::success(id);
}

Result<void> VersionedWorld::restore_entity(EntityRecord record) {
    const EntityRecord copy = record;
    const auto restored = registry_.restore(std::move(record));
    if (!restored) return restored;

    const auto committed = commit({WorldChange{WorldChangeKind::EntityCreated, EntityCreated{copy}}});
    if (!committed) {
        registry_.remove(copy.id);
        return committed;
    }
    return Result<void>::success();
}

Result<EntityRecord> VersionedWorld::remove_entity(EntityId id) {
    const auto prior_zone = topology_.zone_of(id);
    const auto removed = registry_.remove(id);
    if (!removed) return removed;

    EntityRecord record = removed.value();
    std::vector<WorldChange> changes;
    if (prior_zone.has_value()) {
        const auto cleared = topology_.clear_entity(id);
        if (!cleared) {
            registry_.restore(record);
            return Result<EntityRecord>::failure(cleared.error().code, cleared.error().message);
        }
        changes.push_back(WorldChange{
            WorldChangeKind::EntityZoneChanged,
            EntityZoneChanged{id, prior_zone, std::nullopt}
        });
    }
    changes.push_back(WorldChange{WorldChangeKind::EntityRemoved, EntityRemoved{record}});

    const auto committed = commit(std::move(changes));
    if (!committed) {
        registry_.restore(record);
        if (prior_zone.has_value()) topology_.place_entity(id, *prior_zone);
        return Result<EntityRecord>::failure(committed.error().code, committed.error().message);
    }
    return Result<EntityRecord>::success(std::move(record));
}

Result<void> VersionedWorld::update_transform(EntityId id, Transform transform) {
    EntityRecord* record = registry_.find_mutable(id);
    if (record == nullptr) {
        return Result<void>::failure(ErrorCode::NotFound, "entity not found");
    }
    if (record->transform == transform) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "transform update produced no state change");
    }

    const Transform before = record->transform;
    record->transform = transform;
    const auto committed = commit({WorldChange{
        WorldChangeKind::EntityTransformUpdated,
        EntityTransformUpdated{id, before, transform}
    }});
    if (!committed) {
        record->transform = before;
        return committed;
    }
    return Result<void>::success();
}

Result<ZoneId> VersionedWorld::create_zone(ZoneCreateInfo info) {
    const auto created = topology_.create_zone(std::move(info));
    if (!created) return created;
    const ZoneId id = created.value();
    const ZoneRecord* record = topology_.find(id);
    if (record == nullptr) {
        return Result<ZoneId>::failure(ErrorCode::InternalError, "created zone missing from topology");
    }
    const auto committed = commit({WorldChange{WorldChangeKind::ZoneCreated, ZoneCreated{*record}}});
    if (!committed) {
        return Result<ZoneId>::failure(committed.error().code, committed.error().message);
    }
    return Result<ZoneId>::success(id);
}

Result<void> VersionedWorld::restore_zone(ZoneRecord zone) {
    const ZoneRecord copy = zone;
    const auto restored = topology_.restore_zone(std::move(zone));
    if (!restored) return restored;
    return commit({WorldChange{WorldChangeKind::ZoneCreated, ZoneCreated{copy}}});
}

Result<void> VersionedWorld::set_zone_parent(ZoneId child, std::optional<ZoneId> parent) {
    const ZoneRecord* zone = topology_.find(child);
    if (zone == nullptr) {
        return Result<void>::failure(ErrorCode::NotFound, "child zone not found");
    }
    const auto before = zone->parent;
    const auto changed = topology_.set_parent(child, parent);
    if (!changed) return changed;
    const auto committed = commit({WorldChange{
        WorldChangeKind::ZoneParentChanged,
        ZoneParentChanged{child, before, parent}
    }});
    if (!committed) {
        topology_.set_parent(child, before);
        return committed;
    }
    return Result<void>::success();
}

Result<void> VersionedWorld::connect_zones(ZoneConnection connection) {
    const ZoneConnection copy = connection;
    const auto connected = topology_.connect(std::move(connection));
    if (!connected) return connected;
    const auto committed = commit({WorldChange{WorldChangeKind::ZonesConnected, ZonesConnected{copy}}});
    if (!committed) {
        topology_.disconnect(copy.from, copy.to);
        return committed;
    }
    return Result<void>::success();
}

Result<void> VersionedWorld::place_entity(EntityId entity, ZoneId zone) {
    if (!registry_.contains(entity)) {
        return Result<void>::failure(ErrorCode::NotFound, "entity not found");
    }
    const auto before = topology_.zone_of(entity);
    const auto placed = topology_.place_entity(entity, zone);
    if (!placed) return placed;
    const auto committed = commit({WorldChange{
        WorldChangeKind::EntityZoneChanged,
        EntityZoneChanged{entity, before, zone}
    }});
    if (!committed) {
        if (before.has_value()) topology_.place_entity(entity, *before);
        else topology_.clear_entity(entity);
        return committed;
    }
    return Result<void>::success();
}

Result<void> VersionedWorld::clear_entity_zone(EntityId entity) {
    if (!registry_.contains(entity)) {
        return Result<void>::failure(ErrorCode::NotFound, "entity not found");
    }
    const auto before = topology_.zone_of(entity);
    if (!before.has_value()) {
        return Result<void>::failure(ErrorCode::NotFound, "entity has no zone placement");
    }
    const auto cleared = topology_.clear_entity(entity);
    if (!cleared) return cleared;
    const auto committed = commit({WorldChange{
        WorldChangeKind::EntityZoneChanged,
        EntityZoneChanged{entity, before, std::nullopt}
    }});
    if (!committed) {
        topology_.place_entity(entity, *before);
        return committed;
    }
    return Result<void>::success();
}

Result<std::vector<WorldDelta>> VersionedWorld::deltas_since(WorldRevision revision) const {
    if (revision > revision_) {
        return Result<std::vector<WorldDelta>>::failure(
            ErrorCode::RevisionConflict,
            "requested revision is ahead of canonical world revision"
        );
    }

    std::vector<WorldDelta> output;
    for (const auto& delta : history_) {
        if (delta.to_revision() > revision) output.push_back(delta);
    }
    return Result<std::vector<WorldDelta>>::success(std::move(output));
}

} // namespace home
