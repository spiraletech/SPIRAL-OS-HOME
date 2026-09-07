#include "home/versioned_world.hpp"

#include <cassert>
#include <variant>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{1}};
    assert(world.revision() == WorldRevision{0});
    assert(world.history().empty());

    EntityCreateInfo avatar{};
    avatar.kind = EntityKind::Avatar;
    avatar.archetype = "spiral.avatar";
    avatar.display_name = "Agnathos";

    const auto created = world.create_entity(avatar);
    assert(created.ok());
    assert(created.value() == EntityId{1});
    assert(world.revision() == WorldRevision{1});
    assert(world.history().size() == 1);

    const WorldDelta& create_delta = world.history().front();
    assert(create_delta.from_revision() == WorldRevision{0});
    assert(create_delta.to_revision() == WorldRevision{1});
    assert(create_delta.changes().size() == 1);
    assert(create_delta.changes()[0].kind == WorldChangeKind::EntityCreated);
    const auto& created_payload = std::get<EntityCreated>(create_delta.changes()[0].payload);
    assert(created_payload.entity.id == EntityId{1});
    assert(created_payload.entity.display_name == "Agnathos");

    Transform moved{};
    moved.position = Vec3Mm{5000, 200, -700};
    moved.rotation = EulerMilliDegrees{0, 45000, 0};
    const auto moved_result = world.update_transform(EntityId{1}, moved);
    assert(moved_result.ok());
    assert(world.revision() == WorldRevision{2});

    const auto& transform_delta = world.history()[1];
    assert(transform_delta.from_revision() == WorldRevision{1});
    assert(transform_delta.to_revision() == WorldRevision{2});
    const auto& transform_payload = std::get<EntityTransformUpdated>(transform_delta.changes()[0].payload);
    assert(transform_payload.entity == EntityId{1});
    assert(transform_payload.before == Transform{});
    assert(transform_payload.after == moved);

    const auto no_op = world.update_transform(EntityId{1}, moved);
    assert(!no_op.ok());
    assert(no_op.error().code == ErrorCode::ValidationFailed);
    assert(world.revision() == WorldRevision{2});
    assert(world.history().size() == 2);

    const auto missing_transform = world.update_transform(EntityId{999}, moved);
    assert(!missing_transform.ok());
    assert(missing_transform.error().code == ErrorCode::NotFound);
    assert(world.revision() == WorldRevision{2});

    const auto removed = world.remove_entity(EntityId{1});
    assert(removed.ok());
    assert(world.revision() == WorldRevision{3});
    assert(!world.entities().contains(EntityId{1}));
    const auto& removed_payload = std::get<EntityRemoved>(world.history()[2].changes()[0].payload);
    assert(removed_payload.entity.id == EntityId{1});
    assert(removed_payload.entity.transform == moved);

    const auto since_zero = world.deltas_since(WorldRevision{0});
    assert(since_zero.ok());
    assert(since_zero.value().size() == 3);

    const auto since_one = world.deltas_since(WorldRevision{1});
    assert(since_one.ok());
    assert(since_one.value().size() == 2);
    assert(since_one.value()[0].from_revision() == WorldRevision{1});

    const auto since_current = world.deltas_since(WorldRevision{3});
    assert(since_current.ok());
    assert(since_current.value().empty());

    const auto future = world.deltas_since(WorldRevision{4});
    assert(!future.ok());
    assert(future.error().code == ErrorCode::RevisionConflict);

    EntityRecord restored{};
    restored.id = EntityId{90};
    restored.world = WorldId{1};
    restored.kind = EntityKind::Npc;
    restored.archetype = "spiral.npc";
    restored.display_name = "Resident";
    const auto restore_result = world.restore_entity(restored);
    assert(restore_result.ok());
    assert(world.revision() == WorldRevision{4});
    assert(world.entities().contains(EntityId{90}));

    const auto invalid_remove = world.remove_entity(EntityId{12345});
    assert(!invalid_remove.ok());
    assert(invalid_remove.error().code == ErrorCode::NotFound);
    assert(world.revision() == WorldRevision{4});

    VersionedWorld invalid_world{WorldId{}};
    const auto invalid_create = invalid_world.create_entity(avatar);
    assert(!invalid_create.ok());
    assert(invalid_world.revision() == WorldRevision{0});
    assert(invalid_world.history().empty());

    return 0;
}
