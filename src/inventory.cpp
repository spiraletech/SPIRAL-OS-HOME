#include "home/inventory.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {
namespace {

bool valid_kind(ItemKind kind) noexcept {
    switch (kind) {
        case ItemKind::Generic:
        case ItemKind::Consumable:
        case ItemKind::Equipment:
        case ItemKind::Key:
        case ItemKind::Quest:
            return true;
    }
    return false;
}

ItemState* find_mutable(std::vector<ItemState>& items, ItemId id) noexcept {
    auto it = std::find_if(items.begin(), items.end(), [&](const ItemState& item) { return item.id == id; });
    return it == items.end() ? nullptr : &*it;
}

void sort_items(std::vector<ItemState>& items) {
    std::sort(items.begin(), items.end(), [](const ItemState& a, const ItemState& b) {
        return a.id.value() < b.id.value();
    });
}

} // namespace

Result<ItemId> InventoryLedger::create_item(std::string archetype_key,
                                            std::string display_name,
                                            ItemKind kind,
                                            std::uint32_t quantity,
                                            std::uint32_t max_stack,
                                            PlayerId owner) {
    if (archetype_key.empty() || display_name.empty()) {
        return Result<ItemId>::failure(ErrorCode::InvalidArgument, "item key and display name must not be empty");
    }
    if (!valid_kind(kind)) {
        return Result<ItemId>::failure(ErrorCode::InvalidArgument, "invalid item kind");
    }
    if (quantity == 0 || max_stack == 0 || quantity > max_stack) {
        return Result<ItemId>::failure(ErrorCode::InvalidArgument, "item quantity must be within stack bounds");
    }

    const ItemId id{next_item_id_++};
    items_.push_back(ItemState{id, std::move(archetype_key), std::move(display_name), kind, quantity, max_stack, owner, 0});
    sort_items(items_);
    return Result<ItemId>::success(id);
}

Result<void> InventoryLedger::set_owner(ItemId id, PlayerId owner) {
    if (!id.valid()) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid item id");
    auto* item = find_mutable(items_, id);
    if (!item) return Result<void>::failure(ErrorCode::NotFound, "item not found");
    if (item->owner == owner) return Result<void>::failure(ErrorCode::ValidationFailed, "item owner did not change");
    item->owner = owner;
    ++item->item_revision;
    return Result<void>::success();
}

Result<void> InventoryLedger::set_quantity(ItemId id, std::uint32_t quantity) {
    if (!id.valid()) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid item id");
    auto* item = find_mutable(items_, id);
    if (!item) return Result<void>::failure(ErrorCode::NotFound, "item not found");
    if (quantity == 0 || quantity > item->max_stack) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "quantity must be within stack bounds");
    }
    if (item->quantity == quantity) return Result<void>::failure(ErrorCode::ValidationFailed, "item quantity did not change");
    item->quantity = quantity;
    ++item->item_revision;
    return Result<void>::success();
}

Result<void> InventoryLedger::add_quantity(ItemId id, std::uint32_t amount) {
    if (amount == 0) return Result<void>::failure(ErrorCode::InvalidArgument, "amount must be positive");
    auto* item = find_mutable(items_, id);
    if (!item) return Result<void>::failure(ErrorCode::NotFound, "item not found");
    if (amount > item->max_stack - item->quantity) {
        return Result<void>::failure(ErrorCode::Overflow, "item stack would exceed max_stack");
    }
    item->quantity += amount;
    ++item->item_revision;
    return Result<void>::success();
}

Result<void> InventoryLedger::consume(ItemId id, std::uint32_t amount) {
    if (amount == 0) return Result<void>::failure(ErrorCode::InvalidArgument, "amount must be positive");
    auto* item = find_mutable(items_, id);
    if (!item) return Result<void>::failure(ErrorCode::NotFound, "item not found");
    if (amount > item->quantity) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "cannot consume more than the current quantity");
    }
    if (amount == item->quantity) {
        return remove_item(id);
    }
    item->quantity -= amount;
    ++item->item_revision;
    return Result<void>::success();
}

Result<void> InventoryLedger::remove_item(ItemId id) {
    if (!id.valid()) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid item id");
    auto it = std::find_if(items_.begin(), items_.end(), [&](const ItemState& item) { return item.id == id; });
    if (it == items_.end()) return Result<void>::failure(ErrorCode::NotFound, "item not found");
    items_.erase(it);
    return Result<void>::success();
}

const ItemState* InventoryLedger::find(ItemId id) const noexcept {
    if (!id.valid()) return nullptr;
    auto it = std::find_if(items_.begin(), items_.end(), [&](const ItemState& item) { return item.id == id; });
    return it == items_.end() ? nullptr : &*it;
}

std::vector<ItemState> InventoryLedger::inventory_for(PlayerId owner) const {
    std::vector<ItemState> result;
    if (!owner.valid()) return result;
    for (const auto& item : items_) {
        if (item.owner == owner) result.push_back(item);
    }
    return result;
}

std::vector<ItemState> InventoryLedger::unowned_items() const {
    std::vector<ItemState> result;
    for (const auto& item : items_) {
        if (!item.owner.valid()) result.push_back(item);
    }
    return result;
}

std::vector<ItemState> InventoryLedger::snapshot() const { return items_; }

} // namespace home
