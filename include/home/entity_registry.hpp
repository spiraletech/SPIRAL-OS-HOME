#pragma once

#include "home/entity.hpp"
#include "home/result.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace home {

class EntityRegistry final {
public:
    explicit EntityRegistry(WorldId world, std::uint64_t first_entity_value = 1) noexcept;

    [[nodiscard]] WorldId world() const noexcept { return world_; }
    [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }
    [[nodiscard]] bool empty() const noexcept { return records_.empty(); }
    [[nodiscard]] bool contains(EntityId id) const noexcept;

    Result<EntityId> create(EntityCreateInfo info);
    Result<void> restore(EntityRecord record);
    Result<EntityRecord> remove(EntityId id);

    [[nodiscard]] const EntityRecord* find(EntityId id) const noexcept;
    [[nodiscard]] EntityRecord* find_mutable(EntityId id) noexcept;
    [[nodiscard]] std::vector<EntityId> ids() const;
    [[nodiscard]] std::vector<EntityId> ids(EntityKind kind) const;
    [[nodiscard]] std::vector<EntityRecord> snapshot() const;

private:
    [[nodiscard]] Result<EntityId> allocate_id();
    void advance_allocator_past(EntityId id) noexcept;

    WorldId world_{};
    std::uint64_t next_entity_value_{1};
    std::vector<EntityRecord> records_{};
};

} // namespace home
