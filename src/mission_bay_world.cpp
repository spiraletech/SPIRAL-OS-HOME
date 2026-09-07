#include "home/mission_bay_world.hpp"

#include <utility>

namespace home {

Result<MissionBayWorldPackage> make_mission_bay_world(WorldId world_id) {
    if (!world_id.valid()) {
        return Result<MissionBayWorldPackage>::failure(
            ErrorCode::InvalidArgument, "Mission Bay world requires a valid WorldId");
    }

    VersionedWorld world{world_id};

    auto mission_bay = world.create_zone(ZoneCreateInfo{
        .kind = ZoneKind::Region,
        .key = "mission-bay",
        .display_name = "Mission Bay",
        .parent = std::nullopt,
        .persistent = true,
    });
    if (!mission_bay) {
        return Result<MissionBayWorldPackage>::failure(mission_bay.error().code, mission_bay.error().message);
    }

    auto crown_point = world.create_zone(ZoneCreateInfo{
        .kind = ZoneKind::District,
        .key = "crown-point",
        .display_name = "Crown Point",
        .parent = mission_bay.value(),
        .persistent = true,
    });
    if (!crown_point) {
        return Result<MissionBayWorldPackage>::failure(crown_point.error().code, crown_point.error().message);
    }

    auto mission_beach = world.create_zone(ZoneCreateInfo{
        .kind = ZoneKind::District,
        .key = "mission-beach",
        .display_name = "Mission Beach",
        .parent = mission_bay.value(),
        .persistent = true,
    });
    if (!mission_beach) {
        return Result<MissionBayWorldPackage>::failure(mission_beach.error().code, mission_beach.error().message);
    }

    auto belmont_park = world.create_zone(ZoneCreateInfo{
        .kind = ZoneKind::Parcel,
        .key = "belmont-park",
        .display_name = "Belmont Park",
        .parent = mission_beach.value(),
        .persistent = true,
    });
    if (!belmont_park) {
        return Result<MissionBayWorldPackage>::failure(belmont_park.error().code, belmont_park.error().message);
    }

    auto bay_waters = world.create_zone(ZoneCreateInfo{
        .kind = ZoneKind::Water,
        .key = "mission-bay-waters",
        .display_name = "Mission Bay Waters",
        .parent = mission_bay.value(),
        .persistent = true,
    });
    if (!bay_waters) {
        return Result<MissionBayWorldPackage>::failure(bay_waters.error().code, bay_waters.error().message);
    }

    const ZoneConnection connections[] = {
        ZoneConnection{crown_point.value(), mission_beach.value(), true, true, "boardwalk"},
        ZoneConnection{mission_beach.value(), belmont_park.value(), true, true, "walk"},
        ZoneConnection{crown_point.value(), bay_waters.value(), true, true, "shore"},
        ZoneConnection{mission_beach.value(), bay_waters.value(), true, true, "shore"},
    };

    for (const auto& connection : connections) {
        auto connected = world.connect_zones(connection);
        if (!connected) {
            return Result<MissionBayWorldPackage>::failure(connected.error().code, connected.error().message);
        }
    }

    return Result<MissionBayWorldPackage>::success(MissionBayWorldPackage{
        .world = std::move(world),
        .mission_bay = mission_bay.value(),
        .crown_point = crown_point.value(),
        .mission_beach = mission_beach.value(),
        .belmont_park = belmont_park.value(),
        .bay_waters = bay_waters.value(),
    });
}

} // namespace home
