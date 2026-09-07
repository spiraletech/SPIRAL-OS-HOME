#pragma once

#include "home/ids.hpp"
#include "home/world_types.hpp"

#include <compare>
#include <cstdint>
#include <string>

namespace home {

enum class EntityKind : std::uint8_t {
    Unknown = 0,
    Avatar,
    Npc,
    Item,
    Vehicle,
    Structure,
    Prop,
    Environment
};

struct EntityRecord final {
    EntityId id{};
    WorldId world{};
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    std::string display_name{};
    Transform transform{};
    bool persistent{true};

    auto operator<=>(const EntityRecord&) const = default;
};

struct EntityCreateInfo final {
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    std::string display_name{};
    Transform transform{};
    bool persistent{true};
};

} // namespace home
