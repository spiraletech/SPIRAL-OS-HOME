#pragma once

#include "home/entity.hpp"
#include "home/revision.hpp"
#include "home/topology.hpp"
#include "home/world_clock.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace home {

enum class WorldChangeKind : std::uint8_t {
    EntityCreated = 0,
    EntityRemoved,
    EntityTransformUpdated,
    ZoneCreated,
    ZoneParentChanged,
    ZonesConnected,
    EntityZoneChanged,
    WorldTimeAdvanced
};

struct EntityCreated final { EntityRecord entity{}; };
struct EntityRemoved final { EntityRecord entity{}; };
struct EntityTransformUpdated final { EntityId entity{}; Transform before{}; Transform after{}; };
struct ZoneCreated final { ZoneRecord zone{}; };
struct ZoneParentChanged final { ZoneId zone{}; std::optional<ZoneId> before{}; std::optional<ZoneId> after{}; };
struct ZonesConnected final { ZoneConnection connection{}; };
struct EntityZoneChanged final { EntityId entity{}; std::optional<ZoneId> before{}; std::optional<ZoneId> after{}; };
struct WorldTimeAdvanced final {
    WorldTime before{};
    WorldTime after{};
    std::uint64_t real_milliseconds{};
    std::uint64_t before_remainder{};
    std::uint64_t after_remainder{};
};

using WorldChangePayload = std::variant<
    EntityCreated, EntityRemoved, EntityTransformUpdated, ZoneCreated,
    ZoneParentChanged, ZonesConnected, EntityZoneChanged, WorldTimeAdvanced
>;

struct WorldChange final {
    WorldChangeKind kind{WorldChangeKind::EntityCreated};
    WorldChangePayload payload{EntityCreated{}};
};

class WorldDelta final {
public:
    WorldDelta(WorldRevision from, WorldRevision to, std::vector<WorldChange> changes)
        : from_(from), to_(to), changes_(std::move(changes)) {}
    [[nodiscard]] WorldRevision from_revision() const noexcept { return from_; }
    [[nodiscard]] WorldRevision to_revision() const noexcept { return to_; }
    [[nodiscard]] const std::vector<WorldChange>& changes() const noexcept { return changes_; }
    [[nodiscard]] bool empty() const noexcept { return changes_.empty(); }
private:
    WorldRevision from_{};
    WorldRevision to_{};
    std::vector<WorldChange> changes_{};
};

} // namespace home
