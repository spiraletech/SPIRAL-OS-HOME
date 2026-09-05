#pragma once

#include <optional>
#include <string>
#include <vector>

#include "spiral/world/world_ids.hpp"

namespace spiral::world {

enum class SpatialKind {
    World,
    Region,
    District,
    Parcel,
    Structure,
    Room,
    Volume,
    Anchor,
    Custom,
};

struct SpatialNode {
    SpatialNodeId id{};
    SpatialKind kind{SpatialKind::Custom};
    std::string custom_kind{};
    std::optional<SpatialNodeId> parent{};
};

struct SpatialGraph {
    std::vector<SpatialNode> nodes{};
};

} // namespace spiral::world
