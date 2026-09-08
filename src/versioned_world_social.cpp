#include "home/versioned_world.hpp"

#include <optional>
#include <utility>

namespace home {

Result<void> VersionedWorld::set_relationship_state(RelationshipState state) {
    const std::uint64_t current_world_minute = clock_.now().milliseconds / 60000ULL;
    if (state.updated_world_minute > current_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "relationship may not reference a future HOME minute");
    }

    std::optional<RelationshipState> before{};
    if (const auto* existing = relationships_.find_relationship(state.from, state.to)) before = *existing;

    RelationshipsLedger staged = relationships_;
    const auto applied = staged.set_relationship(registry_, player_life_, state);
    if (!applied) return applied;

    const auto committed = commit({WorldChange{
        WorldChangeKind::RelationshipStateChanged,
        RelationshipStateChanged{before, state}}});
    if (!committed) return committed;
    relationships_ = std::move(staged);
    return Result<void>::success();
}

Result<void> VersionedWorld::remove_relationship(EntityId from, EntityId to) {
    RelationshipsLedger staged = relationships_;
    const auto removed = staged.remove_relationship(from, to);
    if (!removed) return Result<void>::failure(removed.error().code, removed.error().message);

    const auto committed = commit({WorldChange{
        WorldChangeKind::RelationshipStateChanged,
        RelationshipStateChanged{removed.value(), std::nullopt}}});
    if (!committed) return committed;
    relationships_ = std::move(staged);
    return Result<void>::success();
}

Result<HouseholdId> VersionedWorld::create_household(HouseholdCreateInfo info) {
    const std::uint64_t current_world_minute = clock_.now().milliseconds / 60000ULL;
    if (info.updated_world_minute > current_world_minute) {
        return Result<HouseholdId>::failure(ErrorCode::ValidationFailed, "household may not reference a future HOME minute");
    }

    RelationshipsLedger staged = relationships_;
    const auto created = staged.create_household(registry_, topology_, player_life_, std::move(info));
    if (!created) return created;
    const HouseholdState* after = staged.find_household(created.value());
    if (after == nullptr) return Result<HouseholdId>::failure(ErrorCode::InternalError, "created household missing from social ledger");

    const auto committed = commit({WorldChange{
        WorldChangeKind::HouseholdStateChanged,
        HouseholdStateChanged{std::nullopt, *after}}});
    if (!committed) return Result<HouseholdId>::failure(committed.error().code, committed.error().message);
    relationships_ = std::move(staged);
    return created;
}

Result<void> VersionedWorld::set_household_state(HouseholdState state) {
    const std::uint64_t current_world_minute = clock_.now().milliseconds / 60000ULL;
    if (state.updated_world_minute > current_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "household may not reference a future HOME minute");
    }
    const HouseholdState* existing = relationships_.find_household(state.id);
    if (existing == nullptr) return Result<void>::failure(ErrorCode::NotFound, "household not found");
    const HouseholdState before = *existing;

    RelationshipsLedger staged = relationships_;
    const auto applied = staged.set_household(registry_, topology_, player_life_, state);
    if (!applied) return applied;

    const auto committed = commit({WorldChange{
        WorldChangeKind::HouseholdStateChanged,
        HouseholdStateChanged{before, state}}});
    if (!committed) return committed;
    relationships_ = std::move(staged);
    return Result<void>::success();
}

Result<void> VersionedWorld::dissolve_household(HouseholdId id) {
    RelationshipsLedger staged = relationships_;
    const auto removed = staged.dissolve_household(id);
    if (!removed) return Result<void>::failure(removed.error().code, removed.error().message);

    const auto committed = commit({WorldChange{
        WorldChangeKind::HouseholdStateChanged,
        HouseholdStateChanged{removed.value(), std::nullopt}}});
    if (!committed) return committed;
    relationships_ = std::move(staged);
    return Result<void>::success();
}

} // namespace home
