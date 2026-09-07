#include "home/versioned_world.hpp"

#include <cassert>
#include <variant>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{1}};

    ZoneCreateInfo bay{};
    bay.kind = ZoneKind::Region;
    bay.key = "mission_bay";
    bay.display_name = "Mission Bay";
    const auto bay_id = world.create_zone(bay);
    assert(bay_id.ok());
    assert(bay_id.value() == ZoneId{1});

    ZoneCreateInfo pb{};
    pb.kind = ZoneKind::District;
    pb.key = "pacific_beach";
    pb.display_name = "Pacific Beach";
    pb.parent = bay_id.value();
    const auto pb_id = world.create_zone(pb);
    assert(pb_id.ok());
    assert(pb_id.value() == ZoneId{2});

    ZoneCreateInfo pier{};
    pier.kind = ZoneKind::Parcel;
    pier.key = "crystal_pier";
    pier.display_name = "Crystal Pier";
    pier.parent = pb_id.value();
    const auto pier_id = world.create_zone(pier);
    assert(pier_id.ok());

    ZoneCreateInfo belmont{};
    belmont.kind = ZoneKind::District;
    belmont.key = "belmont";
    belmont.display_name = "Belmont";
    belmont.parent = bay_id.value();
    const auto belmont_id = world.create_zone(belmont);
    assert(belmont_id.ok());

    const auto children = world.topology().children_of(bay_id.value());
    assert(children.size() == 2);
    assert(children[0] == pb_id.value());
    assert(children[1] == belmont_id.value());

    const auto cycle = world.set_zone_parent(bay_id.value(), pier_id.value());
    assert(!cycle.ok());
    assert(cycle.error().code == ErrorCode::ValidationFailed);

    ZoneConnection boardwalk{};
    boardwalk.from = pier_id.value();
    boardwalk.to = belmont_id.value();
    boardwalk.bidirectional = true;
    boardwalk.traversable = true;
    boardwalk.tag = "boardwalk";
    const auto connected = world.connect_zones(boardwalk);
    assert(connected.ok());
    assert(world.topology().directly_traversable(pier_id.value(), belmont_id.value()));
    assert(world.topology().directly_traversable(belmont_id.value(), pier_id.value()));

    const auto neighbors = world.topology().neighbors_of(pier_id.value());
    assert(neighbors.size() == 1);
    assert(neighbors.front() == belmont_id.value());

    EntityCreateInfo avatar{};
    avatar.kind = EntityKind::Avatar;
    avatar.archetype = "spiral.avatar";
    avatar.display_name = "Agnathos";
    const auto avatar_id = world.create_entity(avatar);
    assert(avatar_id.ok());

    const WorldRevision before_place = world.revision();
    const auto first_place = world.place_entity(avatar_id.value(), pier_id.value());
    assert(first_place.ok());
    assert(world.revision() == *before_place.next());
    assert(world.topology().zone_of(avatar_id.value()) == pier_id.value());

    const auto second_place = world.place_entity(avatar_id.value(), belmont_id.value());
    assert(second_place.ok());
    assert(world.topology().zone_of(avatar_id.value()) == belmont_id.value());

    const auto& move_delta = world.history().back();
    assert(move_delta.changes().size() == 1);
    assert(move_delta.changes().front().kind == WorldChangeKind::EntityZoneChanged);
    const auto move = std::get<EntityZoneChanged>(move_delta.changes().front().payload);
    assert(move.before == pier_id.value());
    assert(move.after == belmont_id.value());

    const auto same_zone = world.place_entity(avatar_id.value(), belmont_id.value());
    assert(!same_zone.ok());
    assert(same_zone.error().code == ErrorCode::ValidationFailed);

    EntityCreateInfo ghost{};
    ghost.kind = EntityKind::Npc;
    ghost.archetype = "spiral.ghost";
    const auto ghost_id = world.create_entity(ghost);
    assert(ghost_id.ok());
    const auto missing_zone = world.place_entity(ghost_id.value(), ZoneId{999});
    assert(!missing_zone.ok());
    assert(missing_zone.error().code == ErrorCode::NotFound);

    const auto cleared = world.clear_entity_zone(avatar_id.value());
    assert(cleared.ok());
    assert(!world.topology().zone_of(avatar_id.value()).has_value());

    const auto replaced = world.place_entity(avatar_id.value(), pier_id.value());
    assert(replaced.ok());
    const auto removed = world.remove_entity(avatar_id.value());
    assert(removed.ok());
    assert(!world.topology().zone_of(avatar_id.value()).has_value());
    assert(world.history().back().changes().size() == 2);
    assert(world.history().back().changes()[0].kind == WorldChangeKind::EntityZoneChanged);
    assert(world.history().back().changes()[1].kind == WorldChangeKind::EntityRemoved);

    ZoneCreateInfo duplicate_key = bay;
    const auto duplicate = world.create_zone(duplicate_key);
    assert(!duplicate.ok());
    assert(duplicate.error().code == ErrorCode::AlreadyExists);

    return 0;
}
