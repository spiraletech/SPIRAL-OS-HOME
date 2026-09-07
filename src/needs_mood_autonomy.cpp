#include "home/needs_mood_autonomy.hpp"

#include <algorithm>
#include <utility>

namespace home {

namespace {

bool valid_need_levels(const NeedLevels& needs) noexcept {
    return std::all_of(needs.values.begin(), needs.values.end(), [](std::int32_t value) {
        return value >= kNeedMinimum && value <= kNeedMaximum;
    });
}

bool valid_mood(const MoodState& mood) noexcept {
    return mood.valence >= kMoodValenceMinimum && mood.valence <= kMoodValenceMaximum &&
           mood.arousal >= kMoodArousalMinimum && mood.arousal <= kMoodArousalMaximum;
}

bool valid_autonomy(const AutonomyPolicy& autonomy) noexcept {
    if (autonomy.mode == AutonomyMode::Disabled) {
        return autonomy.initiative_limit_per_hour == 0 &&
               !autonomy.may_change_zone &&
               !autonomy.may_interact_with_entities;
    }
    return autonomy.initiative_limit_per_hour > 0;
}

} // namespace

Result<void> PlayerDynamicsLedger::set(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    PlayerDynamicsState state) {
    if (!state.entity.valid() || !entities.contains(state.entity)) {
        return Result<void>::failure(ErrorCode::NotFound, "player dynamics entity does not exist");
    }
    if (life.find(state.entity) == nullptr) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player dynamics requires canonical player life state");
    }
    if (!valid_need_levels(state.needs)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player need value is outside canonical range");
    }
    if (!valid_mood(state.mood)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player mood value is outside canonical range");
    }
    if (!valid_autonomy(state.autonomy)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player autonomy policy is internally inconsistent");
    }
    if (state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player dynamics sequence must be nonzero");
    }

    const auto it = std::lower_bound(states_.begin(), states_.end(), state.entity,
        [](const PlayerDynamicsState& candidate, EntityId entity) { return candidate.entity < entity; });

    if (it != states_.end() && it->entity == state.entity) {
        if (state.sequence <= it->sequence) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "player dynamics sequence is stale");
        }
        if (state.updated_world_minute < it->updated_world_minute) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "player dynamics world minute regressed");
        }
        *it = std::move(state);
        return Result<void>::success();
    }

    states_.insert(it, std::move(state));
    return Result<void>::success();
}

const PlayerDynamicsState* PlayerDynamicsLedger::find(EntityId entity) const noexcept {
    const auto it = std::lower_bound(states_.begin(), states_.end(), entity,
        [](const PlayerDynamicsState& candidate, EntityId value) { return candidate.entity < value; });
    return it != states_.end() && it->entity == entity ? &*it : nullptr;
}

std::vector<PlayerDynamicsState> PlayerDynamicsLedger::snapshot() const {
    return states_;
}

} // namespace home
