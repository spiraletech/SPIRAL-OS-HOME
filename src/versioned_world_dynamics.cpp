#include "home/versioned_world.hpp"

#include <optional>

namespace home {

Result<void> VersionedWorld::set_player_dynamics_state(PlayerDynamicsState state) {
    const std::uint64_t current_world_minute = clock_.now().milliseconds / 60000ULL;
    if (state.updated_world_minute > current_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player dynamics may not reference a future HOME minute");
    }

    std::optional<PlayerDynamicsState> before{};
    if (const auto* existing = player_dynamics_.find(state.entity)) {
        before = *existing;
    } else if (state.sequence != 1) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "new player dynamics state must begin at sequence 1");
    }

    PlayerDynamicsLedger staged = player_dynamics_;
    const auto applied = staged.set(registry_, player_life_, state);
    if (!applied) return applied;

    const auto committed = commit({WorldChange{
        WorldChangeKind::PlayerDynamicsStateChanged,
        PlayerDynamicsStateChanged{before, state}}});
    if (!committed) return committed;

    player_dynamics_ = std::move(staged);
    return Result<void>::success();
}

Result<AutonomyDecision> VersionedWorld::resolve_player_autonomy(EntityId entity) const {
    const PlayerDynamicsState* state = player_dynamics_.find(entity);
    if (state == nullptr) {
        return Result<AutonomyDecision>::failure(ErrorCode::NotFound, "player dynamics state not found");
    }
    return resolve_autonomy_decision(*state);
}

} // namespace home
