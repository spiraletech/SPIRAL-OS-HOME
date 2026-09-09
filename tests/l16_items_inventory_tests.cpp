#include "home/versioned_world.hpp"

#include <cassert>
#include <string>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{1}};
    assert(world.advance_time(30'000).ok()); // HOME minute 1

    const auto a = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "a", "A"});
    const auto b = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "b", "B"});
    const auto c = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "c", "C"});
    assert(a.ok() && b.ok() && c.ok());
    const auto zone = world.create_zone(ZoneCreateInfo{ZoneKind::Interior, "locker", "Locker"});
    assert(zone.ok());

    for (EntityId id : {a.value(), b.value(), c.value()}) {
        PlayerLifeState life{};
        life.entity = id;
        life.born_world_minute = 0;
        life.updated_world_minute = 1;
        life.sequence = 1;
        assert(world.set_player_life_state(life).ok());
    }

    ItemCreateInfo create{};
    create.archetype_key = "soda_can";
    create.display_name = "Soda Can";
    create.kind = ItemKind::Consumable;
    create.quantity = 3;
    create.max_stack = 6;
    create.durability = 9'000;
    create.owner = a.value();
    create.updated_world_minute = 1;
    const WorldRevision before_create = world.revision();
    const auto item = world.create_item(create);
    assert(item.ok());
    assert(world.revision() == before_create.next().value());
    assert(world.inventory().find(item.value()) != nullptr);
    assert(world.inventory().find(item.value())->owner == a.value());

    ItemState transfer = *world.inventory().find(item.value());
    transfer.owner = b.value();
    transfer.updated_world_minute = 1;
    transfer.sequence = 2;
    assert(world.set_item_state(transfer).ok());
    assert(world.inventory().inventory_for(b.value()).size() == 1);

    ItemState stale = transfer;
    stale.quantity = 2;
    assert(!world.set_item_state(stale).ok());
    assert(world.inventory().find(item.value())->quantity == 3);

    ItemState impossible = transfer;
    impossible.sequence = 3;
    impossible.zone = zone.value();
    assert(!world.set_item_state(impossible).ok());

    ItemState overstack = transfer;
    overstack.sequence = 3;
    overstack.quantity = 7;
    assert(!world.set_item_state(overstack).ok());

    ItemState future = transfer;
    future.sequence = 3;
    future.updated_world_minute = 2;
    assert(!world.set_item_state(future).ok());

    ItemState dropped = transfer;
    dropped.sequence = 3;
    dropped.owner.reset();
    dropped.zone = zone.value();
    dropped.durability = 8'500;
    assert(world.set_item_state(dropped).ok());
    assert(world.inventory().items_in_zone(zone.value()).size() == 1);

    ItemState picked_up = dropped;
    picked_up.sequence = 4;
    picked_up.zone.reset();
    picked_up.owner = b.value();
    assert(world.set_item_state(picked_up).ok());

    const auto snap = world.snapshot();
    assert(snap.items.size() == 1);
    const auto encoded = encode_snapshot(snap);
    assert(encoded.ok());
    assert(encoded.value().find("HOME_SNAPSHOT 10") == 0);
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().items == snap.items);
    const auto restored = VersionedWorld::from_snapshot(decoded.value());
    assert(restored.ok());
    assert(restored.value().inventory().find(item.value()) != nullptr);
    assert(restored.value().inventory().find(item.value())->owner == b.value());

    // v9 compatibility: remove the v10 ITEMS section and downgrade only the header.
    std::string legacy = encoded.value();
    const auto items_start = legacy.find("ITEMS ");
    const auto entities_start = legacy.find("ENTITIES ", items_start);
    assert(items_start != std::string::npos && entities_start != std::string::npos);
    legacy.erase(items_start, entities_start - items_start);
    legacy.replace(0, std::string("HOME_SNAPSHOT 10").size(), "HOME_SNAPSHOT 9");
    const auto legacy_decoded = decode_snapshot(legacy);
    assert(legacy_decoded.ok());
    assert(legacy_decoded.value().items.empty());

    // Direct owner deletion preserves the item but detaches ownership atomically.
    const WorldRevision before_delete = world.revision();
    assert(world.remove_entity(b.value()).ok());
    const auto* detached = world.inventory().find(item.value());
    assert(detached != nullptr && !detached->owner.has_value() && !detached->zone.has_value());
    assert(detached->sequence == 5);
    assert(world.revision() == before_delete.next().value());

    // Transactional owner deletion obeys the same invariant.
    ItemCreateInfo tx_info{};
    tx_info.archetype_key = "keycard";
    tx_info.display_name = "Keycard";
    tx_info.kind = ItemKind::Key;
    tx_info.owner = c.value();
    tx_info.updated_world_minute = 1;
    const auto tx_item = world.create_item(tx_info);
    assert(tx_item.ok());
    WorldTransaction tx{};
    tx.id = WorldTransactionId{1601};
    tx.authority = "l16-test";
    tx.expected_revision = world.revision();
    tx.operations.push_back(TxRemoveEntity{c.value()});
    assert(world.execute(tx).ok());
    const auto* tx_detached = world.inventory().find(tx_item.value());
    assert(tx_detached != nullptr && !tx_detached->owner.has_value());

    // Restored item allocator advances beyond persisted IDs.
    VersionedWorld restored_world = std::move(restored.value());
    ItemCreateInfo after_restore{};
    after_restore.archetype_key = "battery";
    after_restore.display_name = "Battery";
    after_restore.kind = ItemKind::Material;
    after_restore.quantity = 1;
    after_restore.max_stack = 8;
    after_restore.updated_world_minute = 1;
    const auto next_item = restored_world.create_item(after_restore);
    assert(next_item.ok());
    assert(next_item.value().value() > item.value().value());

    return 0;
}
