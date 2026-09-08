#include "home/versioned_world.hpp"

#include <optional>

namespace home {

Result<void> VersionedWorld::set_player_life_state(PlayerLifeState state) {
    const std::uint64_t current_world_minute = clock_.now().milliseconds / 60000ULL;
    if (state.born_world_minute > current_world_minute || state.updated_world_minute > current_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player life state may not reference a future HOME minute");
    }

    if (state.presence == LifePresence::Deceased) {
        if (const auto* dynamics = player_dynamics_.find(state.entity);
            dynamics != nullptr && dynamics->autonomy.mode != AutonomyMode::Disabled) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "active autonomy must be disabled before player life can become deceased");
        }
    }

    std::optional<PlayerLifeState> before{};
    if (const auto* existing = player_life_.find(state.entity)) {
        before = *existing;
    } else if (state.sequence != 1) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "new player life state must begin at sequence 1");
    }

    PlayerLifeLedger staged = player_life_;
    const auto applied = staged.set(registry_, topology_, state);
    if (!applied) return applied;

    const auto committed = commit({WorldChange{
        WorldChangeKind::PlayerLifeStateChanged,
        PlayerLifeStateChanged{before, state}}});
    if (!committed) return committed;

    player_life_ = std::move(staged);
    return Result<void>::success();
}

} // namespace home
