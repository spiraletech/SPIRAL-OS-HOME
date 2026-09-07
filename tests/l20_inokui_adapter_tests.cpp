#include "home/inokui_adapter.hpp"

#include <cassert>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{1}};

    EntityCreateInfo avatar{};
    avatar.kind = EntityKind::Avatar;
    avatar.archetype = "agnathos";
    avatar.display_name = "Agnathos";
    avatar.transform.position = Vec3Mm{1000, 2000, 3000};
    const auto entity_result = world.create_entity(avatar);
    assert(entity_result);
    const EntityId entity = entity_result.value();

    ZoneCreateInfo zone_info{};
    zone_info.kind = ZoneKind::District;
    zone_info.key = "mission-bay";
    zone_info.display_name = "Mission Bay";
    const auto zone_result = world.create_zone(zone_info);
    assert(zone_result);
    const ZoneId zone = zone_result.value();
    assert(world.place_entity(entity, zone));

    InokuiAdapter adapter{world};
    const auto view = adapter.snapshot();
    assert(view.world == WorldId{1});
    assert(view.revision == world.revision());
    assert(view.calendar == world.calendar());
    assert(view.entities.size() == 1);
    assert(view.entities.front().entity == entity);
    assert(view.entities.front().kind == EntityKind::Avatar);
    assert(view.entities.front().archetype == "agnathos");
    assert(view.entities.front().display_name == "Agnathos");
    assert(view.entities.front().transform == avatar.transform);
    assert(view.entities.front().zone == zone);

    const auto before = world.revision();
    const auto second_view = adapter.snapshot();
    assert(second_view == view);
    assert(world.revision() == before);

    return 0;
}
