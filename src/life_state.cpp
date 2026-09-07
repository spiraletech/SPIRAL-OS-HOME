#include "home/life_state.hpp"

#include <algorithm>

namespace home {
namespace {
PlayerLifeState* find_mutable(std::vector<PlayerLifeState>& states, PlayerId player) {
    auto it = std::find_if(states.begin(), states.end(), [&](const PlayerLifeState& state){ return state.player == player; });
    return it == states.end() ? nullptr : &*it;
}
}

Result<void> LifeLedger::create(PlayerLifeState state) {
    if (!state.player.valid() || !state.avatar.valid() || state.display_name.empty()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "player life state requires player, avatar, and display name");
    }
    if (find(state.player) != nullptr) return Result<void>::failure(ErrorCode::AlreadyExists, "player life state already exists");
    if (std::any_of(states_.begin(), states_.end(), [&](const PlayerLifeState& item){ return item.avatar == state.avatar; })) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "avatar already belongs to a player life state");
    }
    states_.push_back(std::move(state));
    std::sort(states_.begin(), states_.end(), [](const PlayerLifeState& a, const PlayerLifeState& b){ return a.player.value() < b.player.value(); });
    return Result<void>::success();
}

Result<void> LifeLedger::set_home(PlayerId player, std::optional<ZoneId> zone) {
    auto* state = find_mutable(states_, player); if (!state) return Result<void>::failure(ErrorCode::NotFound, "player life state not found");
    if (state->home_zone == zone) return Result<void>::failure(ErrorCode::ValidationFailed, "home zone did not change");
    state->home_zone = zone; ++state->life_revision; return Result<void>::success();
}
Result<void> LifeLedger::set_current_zone(PlayerId player, std::optional<ZoneId> zone) {
    auto* state = find_mutable(states_, player); if (!state) return Result<void>::failure(ErrorCode::NotFound, "player life state not found");
    if (state->current_zone == zone) return Result<void>::failure(ErrorCode::ValidationFailed, "current zone did not change");
    state->current_zone = zone; ++state->life_revision; return Result<void>::success();
}
Result<void> LifeLedger::set_active(PlayerId player, bool active) {
    auto* state = find_mutable(states_, player); if (!state) return Result<void>::failure(ErrorCode::NotFound, "player life state not found");
    if (state->active == active) return Result<void>::failure(ErrorCode::ValidationFailed, "active state did not change");
    state->active = active; ++state->life_revision; return Result<void>::success();
}
const PlayerLifeState* LifeLedger::find(PlayerId player) const noexcept {
    auto it = std::find_if(states_.begin(), states_.end(), [&](const PlayerLifeState& state){ return state.player == player; });
    return it == states_.end() ? nullptr : &*it;
}
std::vector<PlayerLifeState> LifeLedger::snapshot() const { return states_; }

} // namespace home
