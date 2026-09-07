#include "home/mission_bay_world.hpp"
#include "home/snapshot.hpp"

#include <cassert>

int main() {
    using namespace home;

    auto invalid = make_mission_bay_world(WorldId{});
    assert(!invalid);
    assert(invalid.error().code == ErrorCode::InvalidArgument);

    auto built = make_mission_bay_world(WorldId{24});
    assert(built);
    auto package = std::move(built).value();

    assert(package.world.world() == WorldId{24});
    assert(package.world.topology().size() == 5);
    assert(package.world.revision().value() == 9);

    const auto* region = package.world.topology().find(package.mission_bay);
    const auto* crown = package.world.topology().find(package.crown_point);
    const auto* beach = package.world.topology().find(package.mission_beach);
    const auto* belmont = package.world.topology().find(package.belmont_park);
    const auto* water = package.world.topology().find(package.bay_waters);

    assert(region && region->key == "mission-bay" && region->kind == ZoneKind::Region);
    assert(crown && crown->parent == package.mission_bay);
    assert(beach && beach->parent == package.mission_bay);
    assert(belmont && belmont->parent == package.mission_beach);
    assert(water && water->parent == package.mission_bay && water->kind == ZoneKind::Water);

    assert(package.world.topology().directly_traversable(package.crown_point, package.mission_beach));
    assert(package.world.topology().directly_traversable(package.mission_beach, package.belmont_park));
    assert(package.world.topology().directly_traversable(package.crown_point, package.bay_waters));
    assert(package.world.topology().directly_traversable(package.mission_beach, package.bay_waters));

    const auto snapshot = package.world.snapshot();
    const auto encoded = encode_snapshot(snapshot);
    assert(encoded);

    auto restored = VersionedWorld::from_snapshot(snapshot);
    assert(restored);
    const auto restored_encoded = encode_snapshot(restored.value().snapshot());
    assert(restored_encoded);
    assert(restored_encoded.value() == encoded.value());

    auto second = make_mission_bay_world(WorldId{24});
    assert(second);
    const auto second_encoded = encode_snapshot(second.value().world.snapshot());
    assert(second_encoded);
    assert(second_encoded.value() == encoded.value());

    return 0;
}
