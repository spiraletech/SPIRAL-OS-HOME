#include "home/theorism.hpp"

#include <algorithm>
#include <utility>

namespace home {

namespace {

template <typename T>
T* find_player(std::vector<T>& values, PlayerId player) noexcept {
    const auto it = std::find_if(values.begin(), values.end(), [player](const T& value) {
        return value.player == player;
    });
    return it == values.end() ? nullptr : &*it;
}

template <typename T>
const T* find_player(const std::vector<T>& values, PlayerId player) noexcept {
    const auto it = std::find_if(values.begin(), values.end(), [player](const T& value) {
        return value.player == player;
    });
    return it == values.end() ? nullptr : &*it;
}

template <typename T>
std::vector<T> ordered_snapshot(std::vector<T> values) {
    std::sort(values.begin(), values.end(), [](const T& a, const T& b) {
        return a.player.value() < b.player.value();
    });
    return values;
}

Result<void> validate_player_and_key(PlayerId player, const std::string& key, const char* label) {
    if (!player.valid()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "player id must be valid");
    }
    if (key.empty()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, std::string{label} + " key must not be empty");
    }
    return Result<void>::success();
}

} // namespace

Result<void> TheorismLedger::set_subclass(PlayerId player, std::string key, std::uint32_t rank) {
    if (auto valid = validate_player_and_key(player, key, "subclass"); !valid) {
        return valid;
    }
    if (rank == 0 || rank > max_subclass_rank) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass rank out of range");
    }
    if (auto* state = find_player(subclasses_, player)) {
        state->key = std::move(key);
        state->rank = rank;
        ++state->subclass_revision;
        return Result<void>::success();
    }
    subclasses_.push_back(SubclassState{player, std::move(key), rank, 1});
    return Result<void>::success();
}

Result<void> TheorismLedger::set_subclass_rank(PlayerId player, std::uint32_t rank) {
    if (rank == 0 || rank > max_subclass_rank) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass rank out of range");
    }
    auto* state = find_player(subclasses_, player);
    if (!state) {
        return Result<void>::failure(ErrorCode::NotFound, "subclass state not found");
    }
    state->rank = rank;
    ++state->subclass_revision;
    return Result<void>::success();
}

Result<void> TheorismLedger::clear_subclass(PlayerId player) {
    const auto old_size = subclasses_.size();
    std::erase_if(subclasses_, [player](const SubclassState& state) { return state.player == player; });
    if (subclasses_.size() == old_size) {
        return Result<void>::failure(ErrorCode::NotFound, "subclass state not found");
    }
    return Result<void>::success();
}

Result<void> TheorismLedger::set_theorism(PlayerId player, std::string key, std::uint32_t conviction) {
    if (auto valid = validate_player_and_key(player, key, "theorism"); !valid) {
        return valid;
    }
    if (conviction > max_conviction) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "theorism conviction out of range");
    }
    if (auto* state = find_player(theorisms_, player)) {
        state->key = std::move(key);
        state->conviction = conviction;
        ++state->theorism_revision;
        return Result<void>::success();
    }
    theorisms_.push_back(TheorismState{player, std::move(key), conviction, 1});
    return Result<void>::success();
}

Result<void> TheorismLedger::set_theorism_conviction(PlayerId player, std::uint32_t conviction) {
    if (conviction > max_conviction) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "theorism conviction out of range");
    }
    auto* state = find_player(theorisms_, player);
    if (!state) {
        return Result<void>::failure(ErrorCode::NotFound, "theorism state not found");
    }
    state->conviction = conviction;
    ++state->theorism_revision;
    return Result<void>::success();
}

Result<void> TheorismLedger::clear_theorism(PlayerId player) {
    const auto old_size = theorisms_.size();
    std::erase_if(theorisms_, [player](const TheorismState& state) { return state.player == player; });
    if (theorisms_.size() == old_size) {
        return Result<void>::failure(ErrorCode::NotFound, "theorism state not found");
    }
    return Result<void>::success();
}

Result<void> TheorismLedger::set_aura(PlayerId player,
                                      std::string signature,
                                      std::uint32_t intensity,
                                      std::int32_t charge) {
    if (auto valid = validate_player_and_key(player, signature, "aura signature"); !valid) {
        return valid;
    }
    if (intensity > max_aura_intensity) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura intensity out of range");
    }
    if (charge < min_aura_charge || charge > max_aura_charge) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura charge out of range");
    }
    if (auto* state = find_player(auras_, player)) {
        state->signature = std::move(signature);
        state->intensity = intensity;
        state->charge = charge;
        ++state->aura_revision;
        return Result<void>::success();
    }
    auras_.push_back(AuraState{player, std::move(signature), intensity, charge, 1});
    return Result<void>::success();
}

Result<void> TheorismLedger::set_aura_intensity(PlayerId player, std::uint32_t intensity) {
    if (intensity > max_aura_intensity) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura intensity out of range");
    }
    auto* state = find_player(auras_, player);
    if (!state) {
        return Result<void>::failure(ErrorCode::NotFound, "aura state not found");
    }
    state->intensity = intensity;
    ++state->aura_revision;
    return Result<void>::success();
}

Result<void> TheorismLedger::set_aura_charge(PlayerId player, std::int32_t charge) {
    if (charge < min_aura_charge || charge > max_aura_charge) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura charge out of range");
    }
    auto* state = find_player(auras_, player);
    if (!state) {
        return Result<void>::failure(ErrorCode::NotFound, "aura state not found");
    }
    state->charge = charge;
    ++state->aura_revision;
    return Result<void>::success();
}

Result<void> TheorismLedger::clear_aura(PlayerId player) {
    const auto old_size = auras_.size();
    std::erase_if(auras_, [player](const AuraState& state) { return state.player == player; });
    if (auras_.size() == old_size) {
        return Result<void>::failure(ErrorCode::NotFound, "aura state not found");
    }
    return Result<void>::success();
}

const SubclassState* TheorismLedger::find_subclass(PlayerId player) const noexcept {
    return find_player(subclasses_, player);
}

const TheorismState* TheorismLedger::find_theorism(PlayerId player) const noexcept {
    return find_player(theorisms_, player);
}

const AuraState* TheorismLedger::find_aura(PlayerId player) const noexcept {
    return find_player(auras_, player);
}

std::vector<SubclassState> TheorismLedger::subclass_snapshot() const {
    return ordered_snapshot(subclasses_);
}

std::vector<TheorismState> TheorismLedger::theorism_snapshot() const {
    return ordered_snapshot(theorisms_);
}

std::vector<AuraState> TheorismLedger::aura_snapshot() const {
    return ordered_snapshot(auras_);
}

} // namespace home
