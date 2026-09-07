#include "home/player_life.hpp"

#include <algorithm>

namespace home {

Result<void> PlayerLifeLedger::set(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    PlayerLifeState state) {
    if (!state.entity.valid() || !entities.contains(state.entity)) {
        return Result<void>::failure(ErrorCode::NotFound, "player life entity does not exist");
    }
    if (state.home_zone.has_value() && !topology.contains(*state.home_zone)) {
        return Result<void>::failure(ErrorCode::NotFound, "player life home zone does not exist");
    }
    if (state.updated_world_minute < state.born_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player life update precedes birth minute");
    }
    if (state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player life sequence must be nonzero");
    }

    const auto it = std::lower_bound(states_.begin(), states_.end(), state.entity,
        [](const PlayerLifeState& candidate, EntityId entity) { return candidate.entity < entity; });
    if (it != states_.end() && it->entity == state.entity) {
        if (state.sequence <= it->sequence) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "player life sequence is stale");
        }
        *it = std::move(state);
        return Result<void>::success();
    }

    states_.insert(it, std::move(state));
    return Result<void>::success();
}

const PlayerLifeState* PlayerLifeLedger::find(EntityId entity) const noexcept {
    const auto it = std::lower_bound(states_.begin(), states_.end(), entity,
        [](const PlayerLifeState& candidate, EntityId value) { return candidate.entity < value; });
    return it != states_.end() && it->entity == entity ? &*it : nullptr;
}

std::vector<PlayerLifeState> PlayerLifeLedger::snapshot() const {
    return states_;
}

} // namespace home
