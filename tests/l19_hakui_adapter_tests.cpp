#include "home/hakui_adapter.hpp"

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

    HakuiAdapter adapter{world};
    const auto view = adapter.snapshot();
    assert(view.world == WorldId{1});
    assert(view.revision == world.revision());
    assert(view.entities.size() == 1);
    assert(view.entities.front().entity == entity);
    assert(view.entities.front().kind == EntityKind::Avatar);
    assert(view.entities.front().archetype == "agnathos");
    assert(view.entities.front().zone == zone);

    HakuiConsequence move{};
    move.kind = HakuiConsequenceKind::TransformChanged;
    move.expected_revision = world.revision();
    move.entity = entity;
    move.transform.position = Vec3Mm{4000, 5000, 6000};
    move.transform.rotation = EulerMilliDegrees{0, 90000, 0};
    const auto move_result = adapter.apply(move);
    assert(move_result);
    assert(move_result.value() == world.revision());
    assert(world.entities().find(entity)->transform == move.transform);

    HakuiConsequence stale = move;
    stale.transform.position = Vec3Mm{7000, 0, 0};
    const auto stale_result = adapter.apply(stale);
    assert(!stale_result);
    assert(stale_result.error().code == ErrorCode::RevisionConflict);

    HakuiConsequence leave{};
    leave.kind = HakuiConsequenceKind::LeftZone;
    leave.expected_revision = world.revision();
    leave.entity = entity;
    assert(adapter.apply(leave));
    assert(!world.topology().zone_of(entity));

    HakuiConsequence enter{};
    enter.kind = HakuiConsequenceKind::EnteredZone;
    enter.expected_revision = world.revision();
    enter.entity = entity;
    enter.zone = zone;
    assert(adapter.apply(enter));
    assert(world.topology().zone_of(entity) == zone);

    HakuiConsequence unknown{};
    unknown.kind = HakuiConsequenceKind::TransformChanged;
    unknown.expected_revision = world.revision();
    unknown.entity = EntityId{9999};
    const auto unknown_result = adapter.apply(unknown);
    assert(!unknown_result);
    assert(unknown_result.error().code == ErrorCode::NotFound);

    return 0;
}
