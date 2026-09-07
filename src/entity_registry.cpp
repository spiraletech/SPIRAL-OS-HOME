#include "home/entity_registry.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {

EntityRegistry::EntityRegistry(WorldId world, std::uint64_t first_entity_value) noexcept
    : world_(world), next_entity_value_(first_entity_value == 0 ? 1 : first_entity_value) {}

bool EntityRegistry::contains(EntityId id) const noexcept {
    return find(id) != nullptr;
}

Result<EntityId> EntityRegistry::allocate_id() {
    if (!world_.valid()) {
        return Result<EntityId>::failure(ErrorCode::ValidationFailed, "registry requires a valid world id");
    }

    while (next_entity_value_ != 0) {
        const EntityId candidate{next_entity_value_};
        if (next_entity_value_ == std::numeric_limits<std::uint64_t>::max()) {
            next_entity_value_ = 0;
        } else {
            ++next_entity_value_;
        }
        if (!contains(candidate)) {
            return Result<EntityId>::success(candidate);
        }
    }

    return Result<EntityId>::failure(ErrorCode::Overflow, "entity id space exhausted");
}

Result<EntityId> EntityRegistry::create(EntityCreateInfo info) {
    if (info.kind == EntityKind::Unknown) {
        return Result<EntityId>::failure(ErrorCode::ValidationFailed, "entity kind must be known");
    }
    if (info.archetype.empty()) {
        return Result<EntityId>::failure(ErrorCode::ValidationFailed, "entity archetype must not be empty");
    }

    auto allocated = allocate_id();
    if (!allocated) {
        return allocated;
    }

    const EntityId id = allocated.value();
    records_.push_back(EntityRecord{
        id,
        world_,
        info.kind,
        std::move(info.archetype),
        std::move(info.display_name),
        info.transform,
        info.persistent
    });
    return Result<EntityId>::success(id);
}

Result<void> EntityRegistry::restore(EntityRecord record) {
    if (!world_.valid()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "registry requires a valid world id");
    }
    if (!record.id.valid()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "restored entity requires a valid id");
    }
    if (record.world != world_) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "entity belongs to another world");
    }
    if (record.kind == EntityKind::Unknown || record.archetype.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "restored entity is incomplete");
    }
    if (contains(record.id)) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "entity id already exists");
    }

    records_.push_back(std::move(record));
    std::sort(records_.begin(), records_.end(), [](const EntityRecord& a, const EntityRecord& b) {
        return a.id.value() < b.id.value();
    });
    advance_allocator_past(records_.back().id);
    return Result<void>::success();
}

Result<EntityRecord> EntityRegistry::remove(EntityId id) {
    if (!id.valid()) {
        return Result<EntityRecord>::failure(ErrorCode::InvalidArgument, "entity id must be valid");
    }

    const auto it = std::find_if(records_.begin(), records_.end(), [id](const EntityRecord& record) {
        return record.id == id;
    });
    if (it == records_.end()) {
        return Result<EntityRecord>::failure(ErrorCode::NotFound, "entity not found");
    }

    EntityRecord removed = std::move(*it);
    records_.erase(it);
    return Result<EntityRecord>::success(std::move(removed));
}

const EntityRecord* EntityRegistry::find(EntityId id) const noexcept {
    const auto it = std::find_if(records_.begin(), records_.end(), [id](const EntityRecord& record) {
        return record.id == id;
    });
    return it == records_.end() ? nullptr : &*it;
}

EntityRecord* EntityRegistry::find_mutable(EntityId id) noexcept {
    const auto it = std::find_if(records_.begin(), records_.end(), [id](const EntityRecord& record) {
        return record.id == id;
    });
    return it == records_.end() ? nullptr : &*it;
}

std::vector<EntityId> EntityRegistry::ids() const {
    std::vector<EntityId> output;
    output.reserve(records_.size());
    for (const auto& record : records_) {
        output.push_back(record.id);
    }
    std::sort(output.begin(), output.end(), [](EntityId a, EntityId b) { return a.value() < b.value(); });
    return output;
}

std::vector<EntityId> EntityRegistry::ids(EntityKind kind) const {
    std::vector<EntityId> output;
    for (const auto& record : records_) {
        if (record.kind == kind) {
            output.push_back(record.id);
        }
    }
    std::sort(output.begin(), output.end(), [](EntityId a, EntityId b) { return a.value() < b.value(); });
    return output;
}

std::vector<EntityRecord> EntityRegistry::snapshot() const {
    auto output = records_;
    std::sort(output.begin(), output.end(), [](const EntityRecord& a, const EntityRecord& b) {
        return a.id.value() < b.id.value();
    });
    return output;
}

void EntityRegistry::advance_allocator_past(EntityId id) noexcept {
    if (next_entity_value_ == 0 || id.value() < next_entity_value_) {
        return;
    }
    if (id.value() == std::numeric_limits<std::uint64_t>::max()) {
        next_entity_value_ = 0;
    } else {
        next_entity_value_ = id.value() + 1;
    }
}

} // namespace home
