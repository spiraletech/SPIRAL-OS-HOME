#pragma once

#include "home/calendar.hpp"
#include "home/entity_registry.hpp"
#include "home/snapshot.hpp"
#include "home/temporal_domain.hpp"
#include "home/topology.hpp"
#include "home/transaction.hpp"
#include "home/world_clock.hpp"
#include "home/world_delta.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace home {

class VersionedWorld final {
public:
    explicit VersionedWorld(WorldId world) noexcept;
    VersionedWorld(WorldId world, WorldClockConfig clock_config) noexcept;
    VersionedWorld(WorldId world, WorldClockConfig clock_config, CalendarConfig calendar_config) noexcept;

    [[nodiscard]] WorldId world() const noexcept { return registry_.world(); }
    [[nodiscard]] WorldRevision revision() const noexcept { return revision_; }
    [[nodiscard]] const EntityRegistry& entities() const noexcept { return registry_; }
    [[nodiscard]] const TopologyRegistry& topology() const noexcept { return topology_; }
    [[nodiscard]] const WorldClock& clock() const noexcept { return clock_; }
    [[nodiscard]] const CalendarConfig& calendar_config() const noexcept { return calendar_config_; }
    [[nodiscard]] CalendarState calendar() const noexcept { return resolve_calendar(clock_.now(), calendar_config_); }
    [[nodiscard]] const TemporalDomainRegistry& temporal_domains() const noexcept { return temporal_domains_; }
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
    Result<WorldTime> advance_time(std::uint64_t real_milliseconds);

    Result<void> add_temporal_domain(TemporalDomain domain) { return temporal_domains_.add(std::move(domain)); }
    Result<void> remove_temporal_domain(TimeDomainId id) { return temporal_domains_.remove(id); }
    [[nodiscard]] Result<ResolvedTemporalTime> resolve_temporal_time() const {
        return temporal_domains_.resolve(clock_.now());
    }

    Result<TransactionReceipt> execute(const WorldTransaction& transaction);
    [[nodiscard]] WorldSnapshot snapshot() const;
    [[nodiscard]] static Result<VersionedWorld> from_snapshot(const WorldSnapshot& snapshot);
    [[nodiscard]] Result<std::vector<WorldDelta>> deltas_since(WorldRevision revision) const;

private:
    Result<void> commit(std::vector<WorldChange> changes);
    EntityRegistry registry_;
    TopologyRegistry topology_;
    WorldClock clock_{};
    CalendarConfig calendar_config_{};
    TemporalDomainRegistry temporal_domains_{};
    WorldRevision revision_{};
    std::vector<WorldDelta> history_{};
};

} // namespace home
