#pragma once

#include "home/entity_registry.hpp"
#include "home/world_delta.hpp"

#include <cstddef>
#include <vector>

namespace home {

class VersionedWorld final {
public:
    explicit VersionedWorld(WorldId world) noexcept;

    [[nodiscard]] WorldId world() const noexcept { return registry_.world(); }
    [[nodiscard]] WorldRevision revision() const noexcept { return revision_; }
    [[nodiscard]] const EntityRegistry& entities() const noexcept { return registry_; }
    [[nodiscard]] const std::vector<WorldDelta>& history() const noexcept { return history_; }

    Result<EntityId> create_entity(EntityCreateInfo info);
    Result<void> restore_entity(EntityRecord record);
    Result<EntityRecord> remove_entity(EntityId id);
    Result<void> update_transform(EntityId id, Transform transform);

    [[nodiscard]] Result<std::vector<WorldDelta>> deltas_since(WorldRevision revision) const;

private:
    Result<void> commit(std::vector<WorldChange> changes);

    EntityRegistry registry_;
    WorldRevision revision_{};
    std::vector<WorldDelta> history_{};
};

} // namespace home
