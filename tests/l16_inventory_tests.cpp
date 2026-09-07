#include "home/inventory.hpp"

#include <cassert>

int main() {
    using namespace home;

    InventoryLedger inventory;
    const PlayerId player_a{10};
    const PlayerId player_b{20};

    auto created = inventory.create_item("medkit", "Med Kit", ItemKind::Consumable, 2, 5, player_a);
    assert(created.ok());
    const ItemId medkit = created.value();

    const auto* initial = inventory.find(medkit);
    assert(initial != nullptr);
    assert(initial->quantity == 2);
    assert(initial->max_stack == 5);
    assert(initial->owner == player_a);
    assert(initial->item_revision == 0);

    assert(inventory.add_quantity(medkit, 2).ok());
    const auto* stacked = inventory.find(medkit);
    assert(stacked != nullptr);
    assert(stacked->quantity == 4);
    assert(stacked->item_revision == 1);

    auto overflow = inventory.add_quantity(medkit, 2);
    assert(!overflow.ok());
    assert(overflow.error().code == ErrorCode::Overflow);

    assert(inventory.consume(medkit, 1).ok());
    const auto* consumed = inventory.find(medkit);
    assert(consumed != nullptr);
    assert(consumed->quantity == 3);
    assert(consumed->item_revision == 2);

    assert(inventory.set_owner(medkit, player_b).ok());
    assert(inventory.inventory_for(player_a).empty());
    const auto player_b_inventory = inventory.inventory_for(player_b);
    assert(player_b_inventory.size() == 1);
    assert(player_b_inventory.front().id == medkit);
    assert(player_b_inventory.front().item_revision == 3);

    auto duplicate_owner = inventory.set_owner(medkit, player_b);
    assert(!duplicate_owner.ok());
    assert(duplicate_owner.error().code == ErrorCode::ValidationFailed);

    auto loose = inventory.create_item("tower_key", "Tower Key", ItemKind::Key, 1, 1);
    assert(loose.ok());
    assert(inventory.unowned_items().size() == 1);

    auto bad_stack = inventory.create_item("ammo", "Ammo", ItemKind::Generic, 6, 5, player_a);
    assert(!bad_stack.ok());
    assert(bad_stack.error().code == ErrorCode::InvalidArgument);

    assert(inventory.consume(medkit, 3).ok());
    assert(inventory.find(medkit) == nullptr);

    const auto snapshot = inventory.snapshot();
    assert(snapshot.size() == 1);
    assert(snapshot.front().id == loose.value());

    return 0;
}
