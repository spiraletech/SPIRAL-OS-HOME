#pragma once

#include <cstdint>
#include <string>

#include "spiral/world/world_ids.hpp"

namespace spiral::world {

enum class Handedness {
    Left,
    Right,
};

enum class Axis {
    PositiveX,
    PositiveY,
    PositiveZ,
    NegativeX,
    NegativeY,
    NegativeZ,
};

struct CoordinateFrame {
    std::string linear_unit{"meter"};
    Handedness handedness{Handedness::Right};
    Axis up_axis{Axis::PositiveY};
    OriginId origin_id{};
    std::uint64_t rebase_generation{0};
};

} // namespace spiral::world
