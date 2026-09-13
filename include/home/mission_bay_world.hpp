#pragma once

#include "home/result.hpp"
#include "home/versioned_world.hpp"

namespace home {

struct MissionBayZoneIds final {
    ZoneId mission_bay{};
    ZoneId superbloom_east_edge{};
    ZoneId tecolote{};
    ZoneId mission_bay_golf{};
    ZoneId mike_gotch_bridge{};
    ZoneId campland{};
    ZoneId kendall_frost_marsh{};
    ZoneId crown_point{};
    ZoneId fanuel{};
    ZoneId catamaran{};
    ZoneId pacific_beach{};
    ZoneId garnet_avenue{};
    ZoneId mission_boulevard{};
    ZoneId crystal_pier{};
    ZoneId mission_beach{};
    ZoneId belmont_park{};
    ZoneId belmont_broken_coaster_edge{};
    ZoneId mission_bay_waters{};
    ZoneId fiesta_island{};
};

struct MissionBayLandmarkIds final {
    EntityId superbloom_boundary{};
    EntityId belmont_boundary{};
};

struct MissionBayWorldPackage final {
    VersionedWorld world;
    MissionBayZoneIds zones{};
    MissionBayLandmarkIds landmarks{};
    EventId halloween_event{1};
};

// Deterministic canonical HOME package for the compressed Mission Bay v0.1 map.
// This builds world truth only; HAKUI/INOKUI/ENTROKOI/XENON/GUFF remain separate.
[[nodiscard]] Result<MissionBayWorldPackage> make_mission_bay_world(WorldId world_id);

} // namespace home
