#include "home/entrokoi_adapter.hpp"

#include <cassert>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{1}};

    ZoneCreateInfo district_info{};
    district_info.kind = ZoneKind::District;
    district_info.key = "mission-bay";
    district_info.display_name = "Mission Bay";
    const auto district_result = world.create_zone(district_info);
    assert(district_result);
    const ZoneId district = district_result.value();

    ZoneCreateInfo room_info{};
    room_info.kind = ZoneKind::Room;
    room_info.key = "studio";
    room_info.display_name = "Studio";
    room_info.parent = district;
    const auto room_result = world.create_zone(room_info);
    assert(room_result);
    const ZoneId room = room_result.value();

    ZoneConnection connection{};
    connection.from = district;
    connection.to = room;
    connection.bidirectional = true;
    connection.traversable = true;
    connection.tag = "door";
    assert(world.connect_zones(connection));

    EntityCreateInfo avatar{};
    avatar.kind = EntityKind::Avatar;
    avatar.archetype = "agnathos";
    avatar.display_name = "Agnathos";
    avatar.transform.position = Vec3Mm{1000, 2000, 3000};
    const auto entity_result = world.create_entity(avatar);
    assert(entity_result);
    const EntityId entity = entity_result.value();
    assert(world.place_entity(entity, room));

    EntrokoiAdapter adapter{world};
    const auto view = adapter.snapshot();

    assert(view.world == WorldId{1});
    assert(view.revision == world.revision());
    assert(view.calendar == world.calendar());
    assert(view.zones == world.topology().snapshot_zones());
    assert(view.connections == world.topology().snapshot_connections());
    assert(view.entities.size() == 1);
    assert(view.entities.front().entity == entity);
    assert(view.entities.front().kind == EntityKind::Avatar);
    assert(view.entities.front().archetype == "agnathos");
    assert(view.entities.front().display_name == "Agnathos");
    assert(view.entities.front().transform == avatar.transform);
    assert(view.entities.front().zone == room);

    const auto before = world.revision();
    const auto second_view = adapter.snapshot();
    assert(second_view == view);
    assert(world.revision() == before);

    return 0;
}
