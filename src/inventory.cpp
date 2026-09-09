#include "home/inventory.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {
namespace {

bool valid_kind(ItemKind kind) noexcept {
    return static_cast<unsigned>(kind) <= static_cast<unsigned>(ItemKind::Material);
}

bool valid_owner(const EntityRegistry& entities, const PlayerLifeLedger& life, EntityId owner) noexcept {
    const auto* entity = entities.find(owner);
    return entity != nullptr && entity->kind == EntityKind::Avatar && life.find(owner) != nullptr;
}

} // namespace

Result<void> validate_item_state(const EntityRegistry& entities, const TopologyRegistry& topology,
    const PlayerLifeLedger& life, const ItemState& state) {
    if (!state.id.valid() || state.archetype_key.empty() || state.display_name.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "item identity is invalid");
    }
    if (!valid_kind(state.kind) || state.quantity == 0 || state.max_stack == 0 || state.quantity > state.max_stack) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "item kind or stack state is invalid");
    }
    if (state.durability > kItemDurabilityMaximum || state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "item durability or sequence is invalid");
    }
    if (state.owner && state.zone) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "item cannot be owned and zoned simultaneously");
    }
    if (state.owner && !valid_owner(entities, life, *state.owner)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "item owner requires canonical avatar life state");
    }
    if (state.zone && !topology.contains(*state.zone)) {
        return Result<void>::failure(ErrorCode::NotFound, "item zone does not exist");
    }
    return Result<void>::success();
}

Result<ItemId> InventoryLedger::allocate_item_id() {
    if (next_item_value_ == 0 || next_item_value_ == std::numeric_limits<std::uint64_t>::max()) {
        return Result<ItemId>::failure(ErrorCode::Overflow, "item id space exhausted");
    }
    return Result<ItemId>::success(ItemId{next_item_value_++});
}

void InventoryLedger::advance_allocator_past(ItemId id) noexcept {
    if (id.value() >= next_item_value_ && id.value() != std::numeric_limits<std::uint64_t>::max()) next_item_value_ = id.value() + 1;
}

Result<ItemId> InventoryLedger::create_item(const EntityRegistry& entities, const TopologyRegistry& topology,
    const PlayerLifeLedger& life, ItemCreateInfo info) {
    const auto allocated = allocate_item_id();
    if (!allocated) return allocated;
    ItemState state{allocated.value(), std::move(info.archetype_key), std::move(info.display_name), info.kind,
        info.quantity, info.max_stack, info.durability, info.owner, info.zone, info.updated_world_minute, 1};
    const auto valid = validate_item_state(entities, topology, life, state);
    if (!valid) { --next_item_value_; return Result<ItemId>::failure(valid.error().code, valid.error().message); }
    items_.push_back(std::move(state));
    return allocated;
}

Result<void> InventoryLedger::set_item(const EntityRegistry& entities, const TopologyRegistry& topology,
    const PlayerLifeLedger& life, ItemState state) {
    const auto valid = validate_item_state(entities, topology, life, state); if (!valid) return valid;
    auto it = std::lower_bound(items_.begin(), items_.end(), state.id, [](const ItemState& item, ItemId id){ return item.id < id; });
    if (it == items_.end() || it->id != state.id) return Result<void>::failure(ErrorCode::NotFound, "item not found");
    if (state.sequence <= it->sequence || state.updated_world_minute < it->updated_world_minute) {
        return Result<void>::failure(ErrorCode::RevisionConflict, "item sequence or chronology regressed");
    }
    *it = std::move(state);
    return Result<void>::success();
}

Result<void> InventoryLedger::restore_item(const EntityRegistry& entities, const TopologyRegistry& topology,
    const PlayerLifeLedger& life, ItemState state) {
    const auto valid = validate_item_state(entities, topology, life, state); if (!valid) return valid;
    auto it = std::lower_bound(items_.begin(), items_.end(), state.id, [](const ItemState& item, ItemId id){ return item.id < id; });
    if (it != items_.end() && it->id == state.id) return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate item id");
    advance_allocator_past(state.id);
    items_.insert(it, std::move(state));
    return Result<void>::success();
}

Result<ItemState> InventoryLedger::remove_item(ItemId id) {
    auto it = std::lower_bound(items_.begin(), items_.end(), id, [](const ItemState& item, ItemId value){ return item.id < value; });
    if (it == items_.end() || it->id != id) return Result<ItemState>::failure(ErrorCode::NotFound, "item not found");
    ItemState removed = *it; items_.erase(it); return Result<ItemState>::success(std::move(removed));
}

Result<std::vector<ItemState>> InventoryLedger::purge_owner(EntityId owner, std::uint64_t world_minute) {
    std::vector<ItemState> changed;
    for (auto& item : items_) {
        if (item.owner && *item.owner == owner) {
            if (item.sequence == std::numeric_limits<std::uint64_t>::max()) return Result<std::vector<ItemState>>::failure(ErrorCode::Overflow, "item sequence space exhausted");
            item.owner.reset(); item.updated_world_minute = world_minute; ++item.sequence; changed.push_back(item);
        }
    }
    return Result<std::vector<ItemState>>::success(std::move(changed));
}

const ItemState* InventoryLedger::find(ItemId id) const noexcept {
    auto it = std::lower_bound(items_.begin(), items_.end(), id, [](const ItemState& item, ItemId value){ return item.id < value; });
    return it != items_.end() && it->id == id ? &*it : nullptr;
}
std::vector<ItemState> InventoryLedger::inventory_for(EntityId owner) const { std::vector<ItemState> out; for (const auto& x:items_) if(x.owner&&*x.owner==owner) out.push_back(x); return out; }
std::vector<ItemState> InventoryLedger::items_in_zone(ZoneId zone) const { std::vector<ItemState> out; for (const auto& x:items_) if(x.zone&&*x.zone==zone) out.push_back(x); return out; }
std::vector<ItemState> InventoryLedger::snapshot() const { return items_; }

} // namespace home
