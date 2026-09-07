#include "home/xenon_adapter.hpp"

#include <cassert>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{1}};

    ZoneCreateInfo zone_info{};
    zone_info.kind = ZoneKind::District;
    zone_info.key = "mission-bay";
    zone_info.display_name = "Mission Bay";
    const auto zone_result = world.create_zone(zone_info);
    assert(zone_result);
    const ZoneId zone = zone_result.value();

    EntityCreateInfo radio{};
    radio.kind = EntityKind::Prop;
    radio.archetype = "ether-radio";
    radio.display_name = "Ether Radio";
    radio.transform.position = Vec3Mm{4000, 1500, 0};
    const auto entity_result = world.create_entity(radio);
    assert(entity_result);
    const EntityId entity = entity_result.value();
    assert(world.place_entity(entity, zone));

    XenonAdapter adapter{world};
    const auto view = adapter.snapshot();

    assert(view.world == WorldId{1});
    assert(view.revision == world.revision());
    assert(view.calendar == world.calendar());
    assert(view.zones == world.topology().snapshot_zones());
    assert(view.connections == world.topology().snapshot_connections());
    assert(view.entities.size() == 1);
    assert(view.entities.front().entity == entity);
    assert(view.entities.front().kind == EntityKind::Prop);
    assert(view.entities.front().archetype == "ether-radio");
    assert(view.entities.front().display_name == "Ether Radio");
    assert(view.entities.front().transform == radio.transform);
    assert(view.entities.front().zone == zone);

    const auto before = world.revision();
    const auto second_view = adapter.snapshot();
    assert(second_view == view);
    assert(world.revision() == before);

    return 0;
}
