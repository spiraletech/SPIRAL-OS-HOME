#include "home/theorism.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {
namespace {

bool valid_player(const EntityRegistry& entities, const PlayerLifeLedger& life, EntityId player) {
    const auto* entity = entities.find(player);
    return entity != nullptr && entity->kind == EntityKind::Avatar && life.find(player) != nullptr;
}

std::string_view mood_key(MoodBand band) noexcept {
    switch (band) {
        case MoodBand::Distressed: return "distressed";
        case MoodBand::Low: return "low";
        case MoodBand::Neutral: return "neutral";
        case MoodBand::Positive: return "positive";
        case MoodBand::Elevated: return "elevated";
    }
    return "neutral";
}

} // namespace

std::uint32_t subclass_affinity_for_evidence(std::uint64_t evidence_points) noexcept {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(evidence_points, kSubclassAffinityMaximum));
}

Result<void> validate_subclass_affinity_shape(const SubclassAffinityState& state) {
    if (!state.player.valid() || state.key.empty() || state.evidence_points == 0 || state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass affinity state is malformed");
    }
    if (state.updated_world_minute < state.discovered_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass chronology is invalid");
    }
    if (state.affinity_permille != subclass_affinity_for_evidence(state.evidence_points)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass affinity must match accumulated evidence");
    }
    return Result<void>::success();
}

Result<void> validate_aura_shape(const AuraState& state) {
    if (!state.player.valid() || state.dominant_affinity.empty() || state.signature.empty() || state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura state is malformed");
    }
    if (state.intensity_permille > kAuraIntensityMaximum ||
        state.charge_milli < kAuraChargeMinimum || state.charge_milli > kAuraChargeMaximum ||
        state.coherence_permille > kAuraCoherenceMaximum) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura semantic range is invalid");
    }
    return Result<void>::success();
}

Result<HalaluluResolution> resolve_halalulu(
    EntityId player,
    const std::vector<SubclassAffinityState>& subclasses,
    const PlayerDynamicsState* dynamics) {
    if (!player.valid()) {
        return Result<HalaluluResolution>::failure(ErrorCode::InvalidArgument, "HALALULU requires a valid player");
    }

    const SubclassAffinityState* dominant = nullptr;
    for (const auto& state : subclasses) {
        if (state.player != player) continue;
        const auto valid = validate_subclass_affinity_shape(state);
        if (!valid) return Result<HalaluluResolution>::failure(valid.error().code, valid.error().message);
        if (dominant == nullptr || state.affinity_permille > dominant->affinity_permille ||
            (state.affinity_permille == dominant->affinity_permille && state.key < dominant->key)) {
            dominant = &state;
        }
    }
    if (dominant == nullptr) {
        return Result<HalaluluResolution>::failure(ErrorCode::NotFound, "HALALULU requires at least one discovered subclass affinity");
    }

    std::uint32_t arousal_permille = 0;
    std::int32_t charge = 0;
    MoodBand band = MoodBand::Neutral;
    if (dynamics != nullptr) {
        const auto valid = validate_player_dynamics_shape(*dynamics);
        if (!valid || dynamics->entity != player) {
            return Result<HalaluluResolution>::failure(ErrorCode::ValidationFailed, "HALALULU dynamics state is invalid for player");
        }
        arousal_permille = static_cast<std::uint32_t>(dynamics->mood.arousal / 10);
        charge = dynamics->mood.valence / 10;
        band = dynamics->mood.band;
    }

    const std::uint32_t intensity = std::min<std::uint32_t>(
        kAuraIntensityMaximum,
        dominant->affinity_permille + arousal_permille / 4);
    const std::uint32_t coherence = kAuraCoherenceMaximum - arousal_permille / 2;

    HalaluluResolution result{};
    result.player = player;
    result.dominant_affinity = dominant->key;
    result.signature = "halalulu:" + dominant->key + ":" + std::string(mood_key(band));
    result.intensity_permille = intensity;
    result.charge_milli = charge;
    result.coherence_permille = coherence;
    return Result<HalaluluResolution>::success(std::move(result));
}

const SubclassAffinityState* TheorismLedger::find_subclass(EntityId player, std::string_view key) const noexcept {
    const auto it = std::lower_bound(subclasses_.begin(), subclasses_.end(), std::pair{player, key},
        [](const SubclassAffinityState& state, const auto& needle) {
            if (state.player.value() != needle.first.value()) return state.player.value() < needle.first.value();
            return state.key < needle.second;
        });
    if (it == subclasses_.end() || it->player != player || it->key != key) return nullptr;
    return &*it;
}

std::vector<SubclassAffinityState> TheorismLedger::subclasses_for(EntityId player) const {
    std::vector<SubclassAffinityState> out;
    for (const auto& state : subclasses_) if (state.player == player) out.push_back(state);
    return out;
}

const AuraState* TheorismLedger::find_aura(EntityId player) const noexcept {
    const auto it = std::lower_bound(auras_.begin(), auras_.end(), player,
        [](const AuraState& state, EntityId needle) { return state.player.value() < needle.value(); });
    return it == auras_.end() || it->player != player ? nullptr : &*it;
}

Result<void> TheorismLedger::set_subclass(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    SubclassAffinityState state) {
    const auto valid = validate_subclass_affinity_shape(state);
    if (!valid) return valid;
    if (!valid_player(entities, life, state.player)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass affinity owner requires canonical avatar life state");
    }

    auto it = std::lower_bound(subclasses_.begin(), subclasses_.end(), std::pair{state.player, std::string_view{state.key}},
        [](const SubclassAffinityState& current, const auto& needle) {
            if (current.player.value() != needle.first.value()) return current.player.value() < needle.first.value();
            return current.key < needle.second;
        });
    if (it == subclasses_.end() || it->player != state.player || it->key != state.key) {
        if (state.sequence != 1) return Result<void>::failure(ErrorCode::ValidationFailed, "new subclass affinity must begin at sequence 1");
        subclasses_.insert(it, std::move(state));
        return Result<void>::success();
    }

    if (state.discovered_world_minute != it->discovered_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass discovery minute is immutable");
    }
    if (state.sequence != it->sequence + 1 || state.updated_world_minute < it->updated_world_minute ||
        state.evidence_points <= it->evidence_points) {
        return Result<void>::failure(ErrorCode::RevisionConflict, "subclass update is stale or does not add evidence");
    }
    *it = std::move(state);
    return Result<void>::success();
}

Result<void> TheorismLedger::restore_subclass(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    SubclassAffinityState state) {
    const auto valid = validate_subclass_affinity_shape(state);
    if (!valid) return valid;
    if (!valid_player(entities, life, state.player)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "restored subclass affinity owner is invalid");
    }
    if (find_subclass(state.player, state.key)) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate restored subclass affinity");
    }
    subclasses_.push_back(std::move(state));
    std::sort(subclasses_.begin(), subclasses_.end(), [](const SubclassAffinityState& a, const SubclassAffinityState& b) {
        return a.player.value() != b.player.value() ? a.player.value() < b.player.value() : a.key < b.key;
    });
    return Result<void>::success();
}

Result<void> TheorismLedger::set_aura(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    AuraState state) {
    const auto valid = validate_aura_shape(state);
    if (!valid) return valid;
    if (!valid_player(entities, life, state.player)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura owner requires canonical avatar life state");
    }

    auto it = std::lower_bound(auras_.begin(), auras_.end(), state.player,
        [](const AuraState& current, EntityId player) { return current.player.value() < player.value(); });
    if (it == auras_.end() || it->player != state.player) {
        if (state.sequence != 1) return Result<void>::failure(ErrorCode::ValidationFailed, "new aura must begin at sequence 1");
        auras_.insert(it, std::move(state));
        return Result<void>::success();
    }
    if (state.sequence != it->sequence + 1 || state.updated_world_minute < it->updated_world_minute) {
        return Result<void>::failure(ErrorCode::RevisionConflict, "aura update is stale or regressive");
    }
    if (state.dominant_affinity == it->dominant_affinity && state.signature == it->signature &&
        state.intensity_permille == it->intensity_permille && state.charge_milli == it->charge_milli &&
        state.coherence_permille == it->coherence_permille) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "aura update produced no semantic state change");
    }
    *it = std::move(state);
    return Result<void>::success();
}

Result<void> TheorismLedger::restore_aura(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    AuraState state) {
    const auto valid = validate_aura_shape(state);
    if (!valid) return valid;
    if (!valid_player(entities, life, state.player)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "restored aura owner is invalid");
    }
    if (find_aura(state.player)) return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate restored aura");
    auras_.push_back(std::move(state));
    std::sort(auras_.begin(), auras_.end(), [](const AuraState& a, const AuraState& b) {
        return a.player.value() < b.player.value();
    });
    return Result<void>::success();
}

TheorismPurge TheorismLedger::purge_player(EntityId player) {
    TheorismPurge out{};
    auto it = subclasses_.begin();
    while (it != subclasses_.end()) {
        if (it->player == player) {
            out.subclasses.push_back(*it);
            it = subclasses_.erase(it);
        } else {
            ++it;
        }
    }
    auto aura = std::lower_bound(auras_.begin(), auras_.end(), player,
        [](const AuraState& state, EntityId needle) { return state.player.value() < needle.value(); });
    if (aura != auras_.end() && aura->player == player) {
        out.aura = *aura;
        auras_.erase(aura);
    }
    return out;
}

std::vector<SubclassAffinityState> TheorismLedger::subclass_snapshot() const { return subclasses_; }
std::vector<AuraState> TheorismLedger::aura_snapshot() const { return auras_; }

} // namespace home
