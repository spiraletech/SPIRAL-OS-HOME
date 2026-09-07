#pragma once

#include "home/entity.hpp"
#include "home/revision.hpp"

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace home {

enum class WorldChangeKind : std::uint8_t {
    EntityCreated = 0,
    EntityRemoved,
    EntityTransformUpdated
};

struct EntityCreated final {
    EntityRecord entity{};
};

struct EntityRemoved final {
    EntityRecord entity{};
};

struct EntityTransformUpdated final {
    EntityId entity{};
    Transform before{};
    Transform after{};
};

using WorldChangePayload = std::variant<EntityCreated, EntityRemoved, EntityTransformUpdated>;

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
