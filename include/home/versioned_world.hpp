#pragma once

#include "home/entity_registry.hpp"
#include "home/snapshot.hpp"
#include "home/topology.hpp"
#include "home/transaction.hpp"
#include "home/world_delta.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace home {

class VersionedWorld final {
public:
    explicit VersionedWorld(WorldId world) noexcept;

    [[nodiscard]] WorldId world() const noexcept { return registry_.world(); }
    [[nodiscard]] WorldRevision revision() const noexcept { return revision_; }
    [[nodiscard]] const EntityRegistry& entities() const noexcept { return registry_; }
    [[nodiscard]] const TopologyRegistry& topology() const noexcept { return topology_; }
    [[nodiscard]] const std::vector<WorldDelta>& history() const noexcept { return history_; }

    Result<EntityId> create_entity(EntityCreateInfo info);
    Result<void> restore_entity(EntityRecord record);
    Result<EntityRecord> remove_entity(EntityId id);
    Result<void> update_transform(EntityId id, Transform transform);
    Result<ZoneId> create_zone(ZoneCreateInfo info);
    Result<void> restore_zone(ZoneRecord zone);
    Result<void> set_zone_parent(ZoneId child, std::optional<ZoneId> parent);
    Result<void> connect_zones(ZoneConnection connection);
    Result<void> place_entity(EntityId entity, ZoneId zone);
    Result<void> clear_entity_zone(EntityId entity);

    Result<TransactionReceipt> execute(const WorldTransaction& transaction);
    [[nodiscard]] WorldSnapshot snapshot() const;
    [[nodiscard]] static Result<VersionedWorld> from_snapshot(const WorldSnapshot& snapshot);
    [[nodiscard]] Result<std::vector<WorldDelta>> deltas_since(WorldRevision revision) const;

private:
    Result<void> commit(std::vector<WorldChange> changes);

    EntityRegistry registry_;
    TopologyRegistry topology_;
    WorldRevision revision_{};
    std::vector<WorldDelta> history_{};
};

} // namespace home
