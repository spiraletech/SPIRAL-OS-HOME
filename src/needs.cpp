#include "home/needs.hpp"

#include <algorithm>

namespace home {
namespace {
PlayerNeedsState* find_mutable(std::vector<PlayerNeedsState>& states, PlayerId player) {
    auto it = std::find_if(states.begin(), states.end(), [&](const PlayerNeedsState& state) { return state.player == player; });
    return it == states.end() ? nullptr : &*it;
}
bool valid_kind(NeedKind kind) noexcept {
    return static_cast<std::size_t>(kind) < static_cast<std::size_t>(NeedKind::Count);
}
NeedValue* slot(PlayerNeedsState& state, NeedKind kind) noexcept {
    return valid_kind(kind) ? &state.needs[static_cast<std::size_t>(kind)] : nullptr;
}
bool valid_state(const PlayerNeedsState& state) noexcept {
    if (!state.player.valid()) return false;
    for (std::size_t i = 0; i < state.needs.size(); ++i) {
        if (state.needs[i].kind != static_cast<NeedKind>(i) || state.needs[i].value > kNeedMax || state.needs[i].decay_per_tick > kNeedMax) return false;
    }
    return state.mood.valence >= kMoodMin && state.mood.valence <= kMoodMax && state.mood.arousal <= kNeedMax && state.autonomy.initiative_threshold <= kNeedMax;
}
}

PlayerNeedsState default_needs_state(PlayerId player) noexcept {
    PlayerNeedsState state{};
    state.player = player;
    for (std::size_t i = 0; i < state.needs.size(); ++i) state.needs[i] = NeedValue{static_cast<NeedKind>(i), kNeedMax, 0};
    return state;
}

Result<void> NeedsLedger::create(PlayerNeedsState state) {
    if (!valid_state(state)) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid player needs state");
    if (find(state.player)) return Result<void>::failure(ErrorCode::AlreadyExists, "player needs state already exists");
    states_.push_back(std::move(state));
    std::sort(states_.begin(), states_.end(), [](const auto& a, const auto& b) { return a.player.value() < b.player.value(); });
    return Result<void>::success();
}

Result<void> NeedsLedger::set_need(PlayerId player, NeedKind kind, std::uint16_t value) {
    auto* state = find_mutable(states_, player);
    if (!state) return Result<void>::failure(ErrorCode::NotFound, "player needs state not found");
    auto* need = slot(*state, kind);
    if (!need || value > kNeedMax) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid need update");
    if (need->value == value) return Result<void>::failure(ErrorCode::ValidationFailed, "need value did not change");
    need->value = value; ++state->needs_revision; return Result<void>::success();
}

Result<void> NeedsLedger::set_decay(PlayerId player, NeedKind kind, std::uint16_t rate) {
    auto* state = find_mutable(states_, player);
    if (!state) return Result<void>::failure(ErrorCode::NotFound, "player needs state not found");
    auto* need = slot(*state, kind);
    if (!need || rate > kNeedMax) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid need decay");
    if (need->decay_per_tick == rate) return Result<void>::failure(ErrorCode::ValidationFailed, "need decay did not change");
    need->decay_per_tick = rate; ++state->needs_revision; return Result<void>::success();
}

Result<void> NeedsLedger::apply_decay(PlayerId player, std::uint64_t ticks) {
    auto* state = find_mutable(states_, player);
    if (!state) return Result<void>::failure(ErrorCode::NotFound, "player needs state not found");
    if (!ticks) return Result<void>::failure(ErrorCode::InvalidArgument, "decay ticks must be positive");
    bool changed = false;
    for (auto& need : state->needs) {
        const std::uint64_t amount = static_cast<std::uint64_t>(need.decay_per_tick) * ticks;
        const auto next = static_cast<std::uint16_t>(amount >= need.value ? 0 : need.value - amount);
        changed = changed || next != need.value; need.value = next;
    }
    if (!changed) return Result<void>::failure(ErrorCode::ValidationFailed, "decay produced no state change");
    ++state->needs_revision; return Result<void>::success();
}

Result<void> NeedsLedger::set_mood(PlayerId player, MoodState mood) {
    auto* state = find_mutable(states_, player);
    if (!state) return Result<void>::failure(ErrorCode::NotFound, "player needs state not found");
    if (mood.valence < kMoodMin || mood.valence > kMoodMax || mood.arousal > kNeedMax) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid mood state");
    if (state->mood == mood) return Result<void>::failure(ErrorCode::ValidationFailed, "mood did not change");
    state->mood = mood; ++state->needs_revision; return Result<void>::success();
}

Result<void> NeedsLedger::set_autonomy(PlayerId player, AutonomyState value) {
    auto* state = find_mutable(states_, player);
    if (!state) return Result<void>::failure(ErrorCode::NotFound, "player needs state not found");
    if (value.initiative_threshold > kNeedMax) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid autonomy threshold");
    if (state->autonomy == value) return Result<void>::failure(ErrorCode::ValidationFailed, "autonomy did not change");
    state->autonomy = value; ++state->needs_revision; return Result<void>::success();
}

const PlayerNeedsState* NeedsLedger::find(PlayerId player) const noexcept {
    auto it = std::find_if(states_.begin(), states_.end(), [&](const auto& state) { return state.player == player; });
    return it == states_.end() ? nullptr : &*it;
}
std::vector<PlayerNeedsState> NeedsLedger::snapshot() const { return states_; }

} // namespace home
