#include "home/mission_bay_world.hpp"

#include <array>
#include <optional>
#include <utility>

namespace home {
namespace {

Result<ZoneId> add_zone(
    VersionedWorld& world,
    ZoneKind kind,
    const char* key,
    const char* display_name,
    std::optional<ZoneId> parent = std::nullopt) {
    return world.create_zone(ZoneCreateInfo{
        .kind = kind,
        .key = key,
        .display_name = display_name,
        .parent = parent,
        .persistent = true,
    });
}

Result<void> add_connection(
    VersionedWorld& world,
    ZoneId from,
    ZoneId to,
    bool traversable,
    const char* tag) {
    return world.connect_zones(ZoneConnection{
        .from = from,
        .to = to,
        .bidirectional = true,
        .traversable = traversable,
        .tag = tag,
    });
}

Result<EntityId> add_boundary(
    VersionedWorld& world,
    ZoneId zone,
    const char* archetype,
    const char* display_name) {
    auto entity = world.create_entity(EntityCreateInfo{
        .kind = EntityKind::Structure,
        .archetype = archetype,
        .display_name = display_name,
        .transform = {},
        .persistent = true,
    });
    if (!entity) return entity;
    const auto placed = world.place_entity(entity.value(), zone);
    if (!placed) {
        return Result<EntityId>::failure(placed.error().code, placed.error().message);
    }
    return entity;
}

} // namespace

Result<MissionBayWorldPackage> make_mission_bay_world(WorldId world_id) {
    if (!world_id.valid()) {
        return Result<MissionBayWorldPackage>::failure(
            ErrorCode::InvalidArgument,
            "Mission Bay world requires a valid WorldId");
    }

    VersionedWorld world{world_id};
    MissionBayZoneIds zones{};

    auto mission_bay = add_zone(world, ZoneKind::Region, "mission-bay", "Mission Bay");
    if (!mission_bay) return Result<MissionBayWorldPackage>::failure(mission_bay.error().code, mission_bay.error().message);
    zones.mission_bay = mission_bay.value();

    auto create_child = [&](ZoneId& out, ZoneKind kind, const char* key, const char* name, ZoneId parent) -> Result<void> {
        auto result = add_zone(world, kind, key, name, parent);
        if (!result) return Result<void>::failure(result.error().code, result.error().message);
        out = result.value();
        return Result<void>::success();
    };

    const auto z1 = create_child(zones.superbloom_east_edge, ZoneKind::Restricted, "superbloom-east-edge", "Superbloom East Edge", zones.mission_bay);
    if (!z1) return Result<MissionBayWorldPackage>::failure(z1.error().code, z1.error().message);
    const auto z2 = create_child(zones.tecolote, ZoneKind::District, "tecolote", "Tecolote", zones.mission_bay);
    if (!z2) return Result<MissionBayWorldPackage>::failure(z2.error().code, z2.error().message);
    const auto z3 = create_child(zones.mission_bay_golf, ZoneKind::Parcel, "mission-bay-golf", "Mission Bay Golf", zones.tecolote);
    if (!z3) return Result<MissionBayWorldPackage>::failure(z3.error().code, z3.error().message);
    const auto z4 = create_child(zones.mike_gotch_bridge, ZoneKind::Threshold, "mike-gotch-bridge", "Mike Gotch Bridge", zones.mission_bay);
    if (!z4) return Result<MissionBayWorldPackage>::failure(z4.error().code, z4.error().message);
    const auto z5 = create_child(zones.campland, ZoneKind::Parcel, "campland", "Campland", zones.mission_bay);
    if (!z5) return Result<MissionBayWorldPackage>::failure(z5.error().code, z5.error().message);
    const auto z6 = create_child(zones.kendall_frost_marsh, ZoneKind::District, "kendall-frost-marsh", "Kendall-Frost Marsh", zones.mission_bay);
    if (!z6) return Result<MissionBayWorldPackage>::failure(z6.error().code, z6.error().message);
    const auto z7 = create_child(zones.crown_point, ZoneKind::District, "crown-point", "Crown Point", zones.mission_bay);
    if (!z7) return Result<MissionBayWorldPackage>::failure(z7.error().code, z7.error().message);
    const auto z8 = create_child(zones.fanuel, ZoneKind::District, "fanuel", "Fanuel", zones.mission_bay);
    if (!z8) return Result<MissionBayWorldPackage>::failure(z8.error().code, z8.error().message);
    const auto z9 = create_child(zones.catamaran, ZoneKind::Parcel, "catamaran", "Catamaran", zones.mission_bay);
    if (!z9) return Result<MissionBayWorldPackage>::failure(z9.error().code, z9.error().message);
    const auto z10 = create_child(zones.pacific_beach, ZoneKind::District, "pacific-beach", "Pacific Beach", zones.mission_bay);
    if (!z10) return Result<MissionBayWorldPackage>::failure(z10.error().code, z10.error().message);
    const auto z11 = create_child(zones.garnet_avenue, ZoneKind::District, "garnet-avenue", "Garnet Avenue", zones.pacific_beach);
    if (!z11) return Result<MissionBayWorldPackage>::failure(z11.error().code, z11.error().message);
    const auto z12 = create_child(zones.mission_boulevard, ZoneKind::District, "mission-boulevard", "Mission Boulevard", zones.pacific_beach);
    if (!z12) return Result<MissionBayWorldPackage>::failure(z12.error().code, z12.error().message);
    const auto z13 = create_child(zones.crystal_pier, ZoneKind::Parcel, "crystal-pier-hotel", "Crystal Pier Hotel", zones.pacific_beach);
    if (!z13) return Result<MissionBayWorldPackage>::failure(z13.error().code, z13.error().message);
    const auto z14 = create_child(zones.mission_beach, ZoneKind::District, "mission-beach", "Mission Beach", zones.mission_bay);
    if (!z14) return Result<MissionBayWorldPackage>::failure(z14.error().code, z14.error().message);
    const auto z15 = create_child(zones.belmont_park, ZoneKind::Parcel, "belmont-park", "Belmont Park", zones.mission_beach);
    if (!z15) return Result<MissionBayWorldPackage>::failure(z15.error().code, z15.error().message);
    const auto z16 = create_child(zones.belmont_broken_coaster_edge, ZoneKind::Restricted, "belmont-broken-coaster-edge", "Belmont Broken Coaster Edge", zones.mission_beach);
    if (!z16) return Result<MissionBayWorldPackage>::failure(z16.error().code, z16.error().message);
    const auto z17 = create_child(zones.mission_bay_waters, ZoneKind::Water, "mission-bay-waters", "Mission Bay Waters", zones.mission_bay);
    if (!z17) return Result<MissionBayWorldPackage>::failure(z17.error().code, z17.error().message);
    const auto z18 = create_child(zones.fiesta_island, ZoneKind::District, "fiesta-island", "Fiesta Island", zones.mission_bay);
    if (!z18) return Result<MissionBayWorldPackage>::failure(z18.error().code, z18.error().message);

    const struct ConnectionSeed { ZoneId from; ZoneId to; bool traversable; const char* tag; } connections[] = {
        {zones.superbloom_east_edge, zones.tecolote, false, "world.boundary.superbloom"},
        {zones.tecolote, zones.mission_bay_golf, true, "route.local"},
        {zones.mission_bay_golf, zones.mike_gotch_bridge, true, "route.bridge-approach"},
        {zones.mike_gotch_bridge, zones.campland, true, "route.bridge"},
        {zones.campland, zones.kendall_frost_marsh, true, "route.marsh"},
        {zones.kendall_frost_marsh, zones.crown_point, true, "route.bayside"},
        {zones.crown_point, zones.fanuel, true, "route.bayside"},
        {zones.fanuel, zones.catamaran, true, "route.bayside"},
        {zones.catamaran, zones.pacific_beach, true, "route.pb"},
        {zones.pacific_beach, zones.garnet_avenue, true, "route.street"},
        {zones.pacific_beach, zones.mission_boulevard, true, "route.street"},
        {zones.mission_boulevard, zones.crystal_pier, true, "route.pier"},
        {zones.mission_boulevard, zones.mission_beach, true, "route.boardwalk"},
        {zones.mission_beach, zones.belmont_park, true, "route.boardwalk"},
        {zones.belmont_park, zones.belmont_broken_coaster_edge, false, "world.boundary.belmont-broken-coaster"},
        {zones.crown_point, zones.mission_bay_waters, true, "shore.transition"},
        {zones.catamaran, zones.mission_bay_waters, true, "shore.transition"},
        {zones.mission_beach, zones.mission_bay_waters, true, "shore.transition"},
        {zones.mission_bay_waters, zones.fiesta_island, false, "world.boundary.fiesta-locked"},
    };

    for (const auto& connection : connections) {
        const auto result = add_connection(world, connection.from, connection.to, connection.traversable, connection.tag);
        if (!result) return Result<MissionBayWorldPackage>::failure(result.error().code, result.error().message);
    }

    EventDefinition halloween{};
    halloween.id = EventId{1};
    halloween.key = "halloween";
    halloween.display_name = "Halloween";
    halloween.kind = EventKind::Holiday;
    halloween.rule = AnnualDateRule{10, 31, 1};
    halloween.priority = 100;
    halloween.affinities = {"calendar.halloween", "theme.haunted"};
    const auto event_result = world.add_event_definition(halloween);
    if (!event_result) return Result<MissionBayWorldPackage>::failure(event_result.error().code, event_result.error().message);

    const std::array<ClimateProfile, 6> climates{{
        ClimateProfile{zones.kendall_frost_marsh, 18000, 420, 300, 0x2401ULL},
        ClimateProfile{zones.crown_point, 19000, 300, 360, 0x2402ULL},
        ClimateProfile{zones.pacific_beach, 19000, 280, 420, 0x2403ULL},
        ClimateProfile{zones.mission_beach, 18500, 320, 460, 0x2404ULL},
        ClimateProfile{zones.mission_bay_waters, 18000, 360, 520, 0x2405ULL},
        ClimateProfile{zones.fiesta_island, 19500, 240, 480, 0x2406ULL},
    }};
    const std::array<std::uint64_t, 6> weather_entropy{{0xB001ULL, 0xB002ULL, 0xB003ULL, 0xB004ULL, 0xB005ULL, 0xB006ULL}};

    for (std::size_t i = 0; i < climates.size(); ++i) {
        const auto climate_result = world.set_climate_profile(climates[i]);
        if (!climate_result) return Result<MissionBayWorldPackage>::failure(climate_result.error().code, climate_result.error().message);
        const auto weather_result = world.evolve_zone_weather(climates[i].zone, weather_entropy[i]);
        if (!weather_result) return Result<MissionBayWorldPackage>::failure(weather_result.error().code, weather_result.error().message);
    }

    MissionBayLandmarkIds landmarks{};
    auto superbloom_boundary = add_boundary(
        world,
        zones.superbloom_east_edge,
        "boundary.superbloom-east-edge",
        "Superbloom East Boundary");
    if (!superbloom_boundary) return Result<MissionBayWorldPackage>::failure(superbloom_boundary.error().code, superbloom_boundary.error().message);
    landmarks.superbloom_boundary = superbloom_boundary.value();

    auto belmont_boundary = add_boundary(
        world,
        zones.belmont_broken_coaster_edge,
        "boundary.belmont-broken-coaster",
        "Belmont Broken Coaster Barrier");
    if (!belmont_boundary) return Result<MissionBayWorldPackage>::failure(belmont_boundary.error().code, belmont_boundary.error().message);
    landmarks.belmont_boundary = belmont_boundary.value();

    return Result<MissionBayWorldPackage>::success(MissionBayWorldPackage{
        .world = std::move(world),
        .zones = zones,
        .landmarks = landmarks,
        .halloween_event = EventId{1},
    });
}

} // namespace home
