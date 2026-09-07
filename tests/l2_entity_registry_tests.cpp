#include "home/entity_registry.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    using namespace home;

    EntityRegistry registry{WorldId{7}};
    assert(registry.empty());

    EntityCreateInfo avatar{};
    avatar.kind = EntityKind::Avatar;
    avatar.archetype = "spiral.avatar";
    avatar.display_name = "Agnathos";
    avatar.transform.position = Vec3Mm{1000, 2000, 3000};

    const auto created = registry.create(avatar);
    assert(created.ok());
    assert(created.value() == EntityId{1});
    assert(registry.size() == 1);
    assert(registry.contains(created.value()));

    const EntityRecord* found = registry.find(created.value());
    assert(found != nullptr);
    assert(found->world == WorldId{7});
    assert(found->kind == EntityKind::Avatar);
    assert(found->display_name == "Agnathos");
    assert(found->transform.position.x == 1000);

    EntityCreateInfo item{};
    item.kind = EntityKind::Item;
    item.archetype = "spiral.skateboard";
    item.display_name = "Skateboard";
    const auto item_id = registry.create(item);
    assert(item_id.ok());
    assert(item_id.value() == EntityId{2});

    const auto avatar_ids = registry.ids(EntityKind::Avatar);
    assert(avatar_ids.size() == 1);
    assert(avatar_ids.front() == EntityId{1});

    EntityRecord restored{};
    restored.id = EntityId{42};
    restored.world = WorldId{7};
    restored.kind = EntityKind::Npc;
    restored.archetype = "spiral.npc";
    restored.display_name = "Resident";
    assert(registry.restore(restored).ok());

    EntityCreateInfo after_restore{};
    after_restore.kind = EntityKind::Prop;
    after_restore.archetype = "spiral.bench";
    const auto after_restore_id = registry.create(after_restore);
    assert(after_restore_id.ok());
    assert(after_restore_id.value() == EntityId{43});

    const auto snapshot = registry.snapshot();
    assert(snapshot.size() == 4);
    assert(snapshot[0].id == EntityId{1});
    assert(snapshot[1].id == EntityId{2});
    assert(snapshot[2].id == EntityId{42});
    assert(snapshot[3].id == EntityId{43});

    const auto duplicate = registry.restore(restored);
    assert(!duplicate.ok());
    assert(duplicate.error().code == ErrorCode::AlreadyExists);

    EntityRecord foreign = restored;
    foreign.id = EntityId{100};
    foreign.world = WorldId{8};
    const auto foreign_result = registry.restore(foreign);
    assert(!foreign_result.ok());
    assert(foreign_result.error().code == ErrorCode::ValidationFailed);

    const auto removed = registry.remove(EntityId{2});
    assert(removed.ok());
    assert(removed.value().kind == EntityKind::Item);
    assert(!registry.contains(EntityId{2}));

    const auto missing = registry.remove(EntityId{999});
    assert(!missing.ok());
    assert(missing.error().code == ErrorCode::NotFound);

    EntityRegistry invalid_world{WorldId{}};
    const auto invalid_create = invalid_world.create(avatar);
    assert(!invalid_create.ok());
    assert(invalid_create.error().code == ErrorCode::ValidationFailed);

    EntityRegistry exhausted{WorldId{1}, std::numeric_limits<std::uint64_t>::max()};
    EntityCreateInfo last{};
    last.kind = EntityKind::Prop;
    last.archetype = "spiral.last";
    const auto last_id = exhausted.create(last);
    assert(last_id.ok());
    assert(last_id.value() == EntityId{std::numeric_limits<std::uint64_t>::max()});
    const auto overflow = exhausted.create(last);
    assert(!overflow.ok());
    assert(overflow.error().code == ErrorCode::Overflow);

    return 0;
}
