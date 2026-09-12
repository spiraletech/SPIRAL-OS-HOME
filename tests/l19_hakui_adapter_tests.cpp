#include "home/hakui_adapter.hpp"

#include <algorithm>
#include <cassert>

namespace {

home::PlayerLifeState life_for(home::EntityId entity) {
    home::PlayerLifeState life{};
    life.entity = entity;
    life.stage = home::LifeStage::Adult;
    life.presence = home::LifePresence::Present;
    life.born_world_minute = 0;
    life.updated_world_minute = 0;
    life.sequence = 1;
    return life;
}

const home::HakuiBodyState* body_for(const home::HakuiFrame& frame, home::EntityId entity) {
    const auto it = std::find_if(frame.bodies.begin(), frame.bodies.end(), [&](const home::HakuiBodyState& body) {
        return body.entity == entity;
    });
    return it == frame.bodies.end() ? nullptr : &*it;
}

} // namespace

int main() {
    using namespace home;

    VersionedWorld world{WorldId{19}};

    const auto avatar = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "agnathos", "Agnathos"});
    assert(avatar.ok());
    assert(world.set_player_life_state(life_for(avatar.value())).ok());

    const auto prop = world.create_entity(EntityCreateInfo{EntityKind::Prop, "board", "Board"});
    assert(prop.ok());

    ZoneCreateInfo crown{};
    crown.kind = ZoneKind::District;
    crown.key = "crown-point";
    crown.display_name = "Crown Point";
    const auto crown_zone = world.create_zone(crown);
    assert(crown_zone.ok());

    ZoneCreateInfo fanuel{};
    fanuel.kind = ZoneKind::District;
    fanuel.key = "fanuel";
    fanuel.display_name = "Fanuel";
    const auto fanuel_zone = world.create_zone(fanuel);
    assert(fanuel_zone.ok());

    assert(world.place_entity(avatar.value(), crown_zone.value()).ok());

    HakuiAdapter adapter{world};
    const HakuiFrame frame = adapter.project();
    assert(frame.world == world.world());
    assert(frame.revision == world.revision());
    assert(frame.world_time == world.clock().now());
    assert(frame.bodies.size() == 2);
    assert(frame.bodies.front().entity.value() < frame.bodies.back().entity.value());

    const auto* avatar_body = body_for(frame, avatar.value());
    assert(avatar_body != nullptr);
    assert(avatar_body->kind == EntityKind::Avatar);
    assert(avatar_body->archetype == "agnathos");
    assert(avatar_body->zone == crown_zone.value());
    assert(avatar_body->life_stage == LifeStage::Adult);
    assert(avatar_body->life_presence == LifePresence::Present);

    const auto* prop_body = body_for(frame, prop.value());
    assert(prop_body != nullptr);
    assert(!prop_body->life_stage.has_value());
    assert(!prop_body->life_presence.has_value());

    // One physical tick may update transform and zone together, but HOME consumes only one revision.
    Transform moved = world.entities().find(avatar.value())->transform;
    moved.position = Vec3Mm{4'000, 5'000, 6'000};
    moved.rotation = EulerMilliDegrees{0, 90'000, 0};

    HakuiConsequenceBatch tick{};
    tick.id = WorldTransactionId{1901};
    tick.expected_revision = frame.revision;
    tick.authority = "hakui.physics";
    tick.consequences.push_back(HakuiTransformConsequence{avatar.value(), moved});
    tick.consequences.push_back(HakuiZoneConsequence{avatar.value(), fanuel_zone.value()});

    const WorldRevision before_tick = world.revision();
    const auto applied = adapter.apply(tick);
    assert(applied.ok());
    assert(applied.value().from_revision == before_tick);
    assert(applied.value().to_revision == before_tick.next().value());
    assert(applied.value().consequence_count == 2);
    assert(world.revision() == before_tick.next().value());
    assert(world.entities().find(avatar.value())->transform == moved);
    assert(world.topology().zone_of(avatar.value()) == fanuel_zone.value());

    const auto& tick_changes = world.history().back().changes();
    assert(tick_changes.size() == 2);
    assert(std::find_if(tick_changes.begin(), tick_changes.end(), [](const WorldChange& change) {
        return change.kind == WorldChangeKind::EntityTransformUpdated;
    }) != tick_changes.end());
    assert(std::find_if(tick_changes.begin(), tick_changes.end(), [](const WorldChange& change) {
        return change.kind == WorldChangeKind::EntityZoneChanged;
    }) != tick_changes.end());

    // Replaying a consequence batch from an old frame cannot mutate newer HOME truth.
    const WorldRevision after_tick = world.revision();
    assert(!adapter.apply(tick).ok());
    assert(world.revision() == after_tick);

    // Only the HAKUI adapter authority namespace is admitted.
    HakuiConsequenceBatch wrong_authority{};
    wrong_authority.id = WorldTransactionId{1902};
    wrong_authority.expected_revision = world.revision();
    wrong_authority.authority = "guff.physics";
    Transform unauthorized_transform = moved;
    unauthorized_transform.position.x += 100;
    wrong_authority.consequences.push_back(HakuiTransformConsequence{avatar.value(), unauthorized_transform});
    const auto unauthorized = adapter.apply(wrong_authority);
    assert(!unauthorized.ok());
    assert(unauthorized.error().code == ErrorCode::ValidationFailed);
    assert(world.entities().find(avatar.value())->transform == moved);

    // Duplicate writes to the same physical field are ambiguous and consume no revision.
    HakuiConsequenceBatch duplicate{};
    duplicate.id = WorldTransactionId{1903};
    duplicate.expected_revision = world.revision();
    duplicate.authority = "hakui.physics";
    Transform duplicate_a = moved;
    duplicate_a.position.x += 10;
    Transform duplicate_b = moved;
    duplicate_b.position.x += 20;
    duplicate.consequences.push_back(HakuiTransformConsequence{avatar.value(), duplicate_a});
    duplicate.consequences.push_back(HakuiTransformConsequence{avatar.value(), duplicate_b});
    const WorldRevision before_duplicate = world.revision();
    assert(!adapter.apply(duplicate).ok());
    assert(world.revision() == before_duplicate);
    assert(world.entities().find(avatar.value())->transform == moved);

    // No-op physical consequences are rejected rather than minting fake world revisions.
    HakuiConsequenceBatch noop{};
    noop.id = WorldTransactionId{1904};
    noop.expected_revision = world.revision();
    noop.authority = "hakui.physics";
    noop.consequences.push_back(HakuiTransformConsequence{avatar.value(), moved});
    const WorldRevision before_noop = world.revision();
    assert(!adapter.apply(noop).ok());
    assert(world.revision() == before_noop);

    // If one consequence is invalid, the entire physical tick remains uncommitted.
    HakuiConsequenceBatch atomic_failure{};
    atomic_failure.id = WorldTransactionId{1905};
    atomic_failure.expected_revision = world.revision();
    atomic_failure.authority = "hakui.physics";
    Transform should_not_land = moved;
    should_not_land.position.z += 777;
    atomic_failure.consequences.push_back(HakuiTransformConsequence{avatar.value(), should_not_land});
    atomic_failure.consequences.push_back(HakuiZoneConsequence{avatar.value(), ZoneId{999'999}});
    const WorldRevision before_atomic_failure = world.revision();
    const auto failed_tick = adapter.apply(atomic_failure);
    assert(!failed_tick.ok());
    assert(failed_tick.error().code == ErrorCode::NotFound);
    assert(world.revision() == before_atomic_failure);
    assert(world.entities().find(avatar.value())->transform == moved);
    assert(world.topology().zone_of(avatar.value()) == fanuel_zone.value());

    // HAKUI may clear physical zone placement through the same typed batch boundary.
    HakuiConsequenceBatch leave{};
    leave.id = WorldTransactionId{1906};
    leave.expected_revision = world.revision();
    leave.authority = "hakui.traversal";
    leave.consequences.push_back(HakuiZoneConsequence{avatar.value(), std::nullopt});
    assert(adapter.apply(leave).ok());
    assert(!world.topology().zone_of(avatar.value()).has_value());

    // Unknown entities cannot be materialized into HOME by the adapter.
    HakuiConsequenceBatch unknown{};
    unknown.id = WorldTransactionId{1907};
    unknown.expected_revision = world.revision();
    unknown.authority = "hakui.physics";
    unknown.consequences.push_back(HakuiTransformConsequence{EntityId{999'999}, moved});
    const auto unknown_result = adapter.apply(unknown);
    assert(!unknown_result.ok());
    assert(unknown_result.error().code == ErrorCode::NotFound);

    // L19 transport objects are not persisted; only resulting HOME truth survives save/restore.
    const auto encoded = encode_snapshot(world.snapshot());
    assert(encoded.ok());
    assert(encoded.value().find("HAKUI") == std::string::npos);
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    VersionedWorld restored = std::move(restored_result.value());
    HakuiAdapter restored_adapter{restored};
    const HakuiFrame restored_frame = restored_adapter.project();
    assert(restored_frame.revision == restored.revision());
    assert(body_for(restored_frame, avatar.value()) != nullptr);
    assert(!body_for(restored_frame, avatar.value())->zone.has_value());

    return 0;
}
