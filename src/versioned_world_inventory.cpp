#include "home/versioned_world.hpp"

#include <optional>
#include <utility>

namespace home {

Result<ItemId> VersionedWorld::create_item(ItemCreateInfo info) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (info.updated_world_minute > now) return Result<ItemId>::failure(ErrorCode::ValidationFailed, "item may not reference a future HOME minute");
    InventoryLedger staged = inventory_;
    const auto created = staged.create_item(registry_, topology_, player_life_, std::move(info));
    if (!created) return created;
    const ItemState* state = staged.find(created.value());
    if (!state) return Result<ItemId>::failure(ErrorCode::InternalError, "created item missing from inventory ledger");
    const auto committed = commit({WorldChange{WorldChangeKind::ItemStateChanged, ItemStateChanged{std::nullopt, *state}}});
    if (!committed) return Result<ItemId>::failure(committed.error().code, committed.error().message);
    inventory_ = std::move(staged);
    return created;
}

Result<void> VersionedWorld::set_item_state(ItemState state) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (state.updated_world_minute > now) return Result<void>::failure(ErrorCode::ValidationFailed, "item may not reference a future HOME minute");
    const ItemState* existing = inventory_.find(state.id);
    if (!existing) return Result<void>::failure(ErrorCode::NotFound, "item not found");
    const ItemState before = *existing;
    InventoryLedger staged = inventory_;
    const auto applied = staged.set_item(registry_, topology_, player_life_, state);
    if (!applied) return applied;
    const auto committed = commit({WorldChange{WorldChangeKind::ItemStateChanged, ItemStateChanged{before, state}}});
    if (!committed) return committed;
    inventory_ = std::move(staged);
    return Result<void>::success();
}

Result<void> VersionedWorld::remove_item(ItemId id) {
    InventoryLedger staged = inventory_;
    const auto removed = staged.remove_item(id);
    if (!removed) return Result<void>::failure(removed.error().code, removed.error().message);
    const auto committed = commit({WorldChange{WorldChangeKind::ItemStateChanged, ItemStateChanged{removed.value(), std::nullopt}}});
    if (!committed) return committed;
    inventory_ = std::move(staged);
    return Result<void>::success();
}

} // namespace home
