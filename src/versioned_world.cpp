#include "home/versioned_world.hpp"

#include <algorithm>
#include <utility>

namespace home {

VersionedWorld::VersionedWorld(WorldId world) noexcept : registry_(world) {}

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
    if (!created) {
        return created;
    }

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
    if (!restored) {
        return restored;
    }

    const auto committed = commit({WorldChange{WorldChangeKind::EntityCreated, EntityCreated{copy}}});
    if (!committed) {
        registry_.remove(copy.id);
        return committed;
    }
    return Result<void>::success();
}

Result<EntityRecord> VersionedWorld::remove_entity(EntityId id) {
    const auto removed = registry_.remove(id);
    if (!removed) {
        return removed;
    }

    EntityRecord record = removed.value();
    const auto committed = commit({WorldChange{WorldChangeKind::EntityRemoved, EntityRemoved{record}}});
    if (!committed) {
        registry_.restore(record);
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

Result<std::vector<WorldDelta>> VersionedWorld::deltas_since(WorldRevision revision) const {
    if (revision > revision_) {
        return Result<std::vector<WorldDelta>>::failure(
            ErrorCode::RevisionConflict,
            "requested revision is ahead of canonical world revision"
        );
    }

    std::vector<WorldDelta> output;
    for (const auto& delta : history_) {
        if (delta.to_revision() > revision) {
            output.push_back(delta);
        }
    }
    return Result<std::vector<WorldDelta>>::success(std::move(output));
}

} // namespace home
