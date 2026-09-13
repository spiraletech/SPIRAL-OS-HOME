#include "home/mission_bay_world.hpp"
#include "home/snapshot.hpp"

#include <algorithm>
#include <cassert>
#include <string>

namespace {

const home::ZoneConnection* find_connection(
    const home::VersionedWorld& world,
    home::ZoneId a,
    home::ZoneId b) {
    static home::ZoneConnection found{};
    const auto connections = world.topology().snapshot_connections();
    const auto it = std::find_if(connections.begin(), connections.end(), [&](const home::ZoneConnection& connection) {
        return (connection.from == a && connection.to == b)
            || (connection.bidirectional && connection.from == b && connection.to == a);
    });
    if (it == connections.end()) return nullptr;
    found = *it;
    return &found;
}

} // namespace

int main() {
    using namespace home;

    const auto invalid = make_mission_bay_world(WorldId{});
    assert(!invalid);
    assert(invalid.error().code == ErrorCode::InvalidArgument);

    auto built = make_mission_bay_world(WorldId{24});
    assert(built);
    auto package = std::move(built).value();
    const auto& z = package.zones;

    assert(package.world.world() == WorldId{24});
    assert(package.world.topology().size() == 19);

    const auto* region = package.world.topology().find(z.mission_bay);
    assert(region && region->kind == ZoneKind::Region && region->key == "mission-bay");
    const auto* superbloom = package.world.topology().find(z.superbloom_east_edge);
    assert(superbloom && superbloom->kind == ZoneKind::Restricted);
    const auto* bridge = package.world.topology().find(z.mike_gotch_bridge);
    assert(bridge && bridge->kind == ZoneKind::Threshold);
    const auto* water = package.world.topology().find(z.mission_bay_waters);
    assert(water && water->kind == ZoneKind::Water);
    const auto* fiesta = package.world.topology().find(z.fiesta_island);
    assert(fiesta && fiesta->parent == z.mission_bay);
    const auto* crystal = package.world.topology().find(z.crystal_pier);
    assert(crystal && crystal->parent == z.pacific_beach && crystal->key == "crystal-pier-hotel");

    // Canonical playable spine.
    assert(package.world.topology().directly_traversable(z.tecolote, z.mission_bay_golf));
    assert(package.world.topology().directly_traversable(z.mission_bay_golf, z.mike_gotch_bridge));
    assert(package.world.topology().directly_traversable(z.mike_gotch_bridge, z.campland));
    assert(package.world.topology().directly_traversable(z.campland, z.kendall_frost_marsh));
    assert(package.world.topology().directly_traversable(z.kendall_frost_marsh, z.crown_point));
    assert(package.world.topology().directly_traversable(z.crown_point, z.fanuel));
    assert(package.world.topology().directly_traversable(z.fanuel, z.catamaran));
    assert(package.world.topology().directly_traversable(z.catamaran, z.pacific_beach));
    assert(package.world.topology().directly_traversable(z.pacific_beach, z.mission_boulevard));
    assert(package.world.topology().directly_traversable(z.mission_boulevard, z.mission_beach));
    assert(package.world.topology().directly_traversable(z.mission_beach, z.belmont_park));

    // PB branches and shoreline transitions are explicit canonical topology.
    assert(package.world.topology().directly_traversable(z.pacific_beach, z.garnet_avenue));
    assert(package.world.topology().directly_traversable(z.mission_boulevard, z.crystal_pier));
    assert(package.world.topology().directly_traversable(z.crown_point, z.mission_bay_waters));
    assert(package.world.topology().directly_traversable(z.catamaran, z.mission_bay_waters));
    assert(package.world.topology().directly_traversable(z.mission_beach, z.mission_bay_waters));

    // Locked edges remain topology facts rather than renderer conventions.
    assert(!package.world.topology().directly_traversable(z.superbloom_east_edge, z.tecolote));
    assert(!package.world.topology().directly_traversable(z.mission_bay_waters, z.fiesta_island));
    assert(!package.world.topology().directly_traversable(z.belmont_park, z.belmont_broken_coaster_edge));

    const auto* superbloom_edge = find_connection(package.world, z.superbloom_east_edge, z.tecolote);
    assert(superbloom_edge && !superbloom_edge->traversable && superbloom_edge->tag == "world.boundary.superbloom");
    const auto* fiesta_edge = find_connection(package.world, z.mission_bay_waters, z.fiesta_island);
    assert(fiesta_edge && !fiesta_edge->traversable && fiesta_edge->tag == "world.boundary.fiesta-locked");
    const auto* belmont_edge = find_connection(package.world, z.belmont_park, z.belmont_broken_coaster_edge);
    assert(belmont_edge && !belmont_edge->traversable && belmont_edge->tag == "world.boundary.belmont-broken-coaster");

    // Diegetic boundary structures are canonical entities located at the locked edges.
    const auto* superbloom_marker = package.world.entities().find(package.landmarks.superbloom_boundary);
    assert(superbloom_marker && superbloom_marker->kind == EntityKind::Structure);
    assert(superbloom_marker->archetype == "boundary.superbloom-east-edge");
    assert(package.world.topology().zone_of(package.landmarks.superbloom_boundary) == z.superbloom_east_edge);
    const auto* belmont_marker = package.world.entities().find(package.landmarks.belmont_boundary);
    assert(belmont_marker && belmont_marker->archetype == "boundary.belmont-broken-coaster");
    assert(package.world.topology().zone_of(package.landmarks.belmont_boundary) == z.belmont_broken_coaster_edge);

    // Mission Bay package seeds semantic event/weather truth for downstream adapters.
    const auto* halloween = package.world.events().find_by_key("halloween");
    assert(halloween && halloween->id == package.halloween_event);
    assert(halloween->kind == EventKind::Holiday);
    assert(std::find(halloween->affinities.begin(), halloween->affinities.end(), "calendar.halloween") != halloween->affinities.end());
    assert(package.world.climates().snapshot().size() == 6);
    assert(package.world.weather().snapshot().size() == 6);
    assert(package.world.climates().find(z.crown_point) != nullptr);
    assert(package.world.weather().find(z.crown_point) != nullptr);
    assert(package.world.climates().find(z.mission_bay_waters) != nullptr);
    assert(package.world.weather().find(z.mission_bay_waters) != nullptr);

    // L24 adds package content only; canonical persistence format stays v12.
    const auto encoded = encode_snapshot(package.world.snapshot());
    assert(encoded);
    assert(encoded.value().rfind("HOME_SNAPSHOT 12", 0) == 0);

    // Snapshot restore retains the entire package truth byte-for-byte.
    auto restored = VersionedWorld::from_snapshot(package.world.snapshot());
    assert(restored);
    const auto restored_encoded = encode_snapshot(restored.value().snapshot());
    assert(restored_encoded);
    assert(restored_encoded.value() == encoded.value());
    assert(!restored.value().topology().directly_traversable(z.belmont_park, z.belmont_broken_coaster_edge));
    const auto* restored_halloween = restored.value().events().find_by_key("halloween");
    assert(restored_halloween && restored_halloween->id == EventId{1});

    // Rebuilding from the package factory is deterministic, not dependent on runtime order.
    auto second = make_mission_bay_world(WorldId{24});
    assert(second);
    const auto second_encoded = encode_snapshot(second.value().world.snapshot());
    assert(second_encoded);
    assert(second_encoded.value() == encoded.value());

    return 0;
}
