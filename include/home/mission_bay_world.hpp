#pragma once

#include "home/result.hpp"
#include "home/versioned_world.hpp"

namespace home {

struct MissionBayWorldPackage final {
    VersionedWorld world;
    ZoneId mission_bay{};
    ZoneId crown_point{};
    ZoneId mission_beach{};
    ZoneId belmont_park{};
    ZoneId bay_waters{};
};

// Builds the deterministic Spiral OS HOME v0.1 Mission Bay starter world.
// HOME owns only canonical topology/state; downstream engines own embodiment,
// rendering, perception, audio and governed reasoning.
[[nodiscard]] Result<MissionBayWorldPackage> make_mission_bay_world(WorldId world_id);

} // namespace home
