#include "home/player_life.hpp"

#include <algorithm>

namespace home {

Result<void> validate_player_life_state(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    const PlayerLifeState& state) {
    if (!state.entity.valid()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player life entity id is invalid");
    }
    const EntityRecord* entity = entities.find(state.entity);
    if (entity == nullptr) {
        return Result<void>::failure(ErrorCode::NotFound, "player life entity does not exist");
    }
    if (entity->kind != EntityKind::Avatar) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player life state may only attach to an avatar entity");
    }
    if (static_cast<unsigned>(state.stage) > static_cast<unsigned>(LifeStage::Elder)
        || static_cast<unsigned>(state.presence) > static_cast<unsigned>(LifePresence::Deceased)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "player life enum value is invalid");
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
    return Result<void>::success();
}

Result<void> PlayerLifeLedger::set(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    PlayerLifeState state) {
    const auto valid = validate_player_life_state(entities, topology, state);
    if (!valid) return valid;

    const auto it = std::lower_bound(states_.begin(), states_.end(), state.entity,
        [](const PlayerLifeState& candidate, EntityId entity) { return candidate.entity < entity; });
    if (it != states_.end() && it->entity == state.entity) {
        if (state.sequence <= it->sequence) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "player life sequence is stale");
        }
        if (state.born_world_minute != it->born_world_minute) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "player life birth minute is immutable");
        }
        if (state.updated_world_minute < it->updated_world_minute) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "player life update minute regressed");
        }
        *it = std::move(state);
        return Result<void>::success();
    }

    states_.insert(it, std::move(state));
    return Result<void>::success();
}

Result<PlayerLifeState> PlayerLifeLedger::remove(EntityId entity) {
    const auto it = std::lower_bound(states_.begin(), states_.end(), entity,
        [](const PlayerLifeState& candidate, EntityId value) { return candidate.entity < value; });
    if (it == states_.end() || it->entity != entity) {
        return Result<PlayerLifeState>::failure(ErrorCode::NotFound, "player life state not found");
    }
    PlayerLifeState removed = *it;
    states_.erase(it);
    return Result<PlayerLifeState>::success(std::move(removed));
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
