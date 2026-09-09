#include "home/versioned_world.hpp"

#include <optional>

namespace home {
namespace {

bool aura_semantics_equal(const AuraState& state, const HalaluluResolution& resolution) noexcept {
    return state.player == resolution.player &&
        state.dominant_affinity == resolution.dominant_affinity &&
        state.signature == resolution.signature &&
        state.intensity_permille == resolution.intensity_permille &&
        state.charge_milli == resolution.charge_milli &&
        state.coherence_permille == resolution.coherence_permille;
}

AuraState aura_from_resolution(
    const HalaluluResolution& resolution,
    std::uint64_t updated_world_minute,
    std::uint64_t sequence) {
    AuraState aura{};
    aura.player = resolution.player;
    aura.dominant_affinity = resolution.dominant_affinity;
    aura.signature = resolution.signature;
    aura.intensity_permille = resolution.intensity_permille;
    aura.charge_milli = resolution.charge_milli;
    aura.coherence_permille = resolution.coherence_permille;
    aura.updated_world_minute = updated_world_minute;
    aura.sequence = sequence;
    return aura;
}

} // namespace

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

    PlayerDynamicsLedger staged_dynamics = player_dynamics_;
    const auto applied = staged_dynamics.set(registry_, player_life_, state);
    if (!applied) return applied;

    TheorismLedger staged_theorism = theorism_;
    std::vector<WorldChange> changes;
    changes.push_back({
        WorldChangeKind::PlayerDynamicsStateChanged,
        PlayerDynamicsStateChanged{before, state}});

    const auto subclasses = staged_theorism.subclasses_for(state.entity);
    if (!subclasses.empty()) {
        const auto resolution = resolve_halalulu(state.entity, subclasses, &state);
        if (!resolution) return Result<void>::failure(resolution.error().code, resolution.error().message);
        const AuraState* current_aura = staged_theorism.find_aura(state.entity);
        if (current_aura == nullptr || !aura_semantics_equal(*current_aura, resolution.value())) {
            std::optional<AuraState> aura_before{};
            if (current_aura != nullptr) aura_before = *current_aura;
            AuraState aura = aura_from_resolution(
                resolution.value(), current_world_minute, current_aura == nullptr ? 1 : current_aura->sequence + 1);
            const auto aura_applied = staged_theorism.set_aura(registry_, player_life_, aura);
            if (!aura_applied) return aura_applied;
            changes.push_back({
                WorldChangeKind::AuraStateChanged,
                AuraStateChanged{aura_before, aura}});
        }
    }

    const auto committed = commit(std::move(changes));
    if (!committed) return committed;

    player_dynamics_ = std::move(staged_dynamics);
    theorism_ = std::move(staged_theorism);
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
