#include "home/versioned_world.hpp"

#include <algorithm>
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
    AuraState state{};
    state.player = resolution.player;
    state.dominant_affinity = resolution.dominant_affinity;
    state.signature = resolution.signature;
    state.intensity_permille = resolution.intensity_permille;
    state.charge_milli = resolution.charge_milli;
    state.coherence_permille = resolution.coherence_permille;
    state.updated_world_minute = updated_world_minute;
    state.sequence = sequence;
    return state;
}

} // namespace

Result<void> VersionedWorld::set_subclass_affinity(SubclassAffinityState state) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (state.discovered_world_minute > now || state.updated_world_minute > now) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass affinity references a future HOME minute");
    }
    const PlayerLifeState* life = player_life_.find(state.player);
    if (life == nullptr) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass affinity requires canonical player life state");
    }
    if (state.discovered_world_minute < life->born_world_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "subclass discovery cannot predate player life");
    }

    std::optional<SubclassAffinityState> before{};
    if (const auto* current = theorism_.find_subclass(state.player, state.key)) before = *current;

    TheorismLedger staged = theorism_;
    const auto applied = staged.set_subclass(registry_, player_life_, state);
    if (!applied) return applied;
    const auto* after = staged.find_subclass(state.player, state.key);
    if (after == nullptr) {
        return Result<void>::failure(ErrorCode::InternalError, "subclass affinity missing after update");
    }

    std::vector<WorldChange> changes;
    changes.push_back({
        WorldChangeKind::SubclassAffinityStateChanged,
        SubclassAffinityStateChanged{before, *after}});

    const auto resolution = resolve_halalulu(
        state.player,
        staged.subclasses_for(state.player),
        player_dynamics_.find(state.player));
    if (!resolution) return Result<void>::failure(resolution.error().code, resolution.error().message);

    const AuraState* current_aura = staged.find_aura(state.player);
    if (current_aura == nullptr || !aura_semantics_equal(*current_aura, resolution.value())) {
        std::optional<AuraState> aura_before{};
        if (current_aura != nullptr) aura_before = *current_aura;
        std::uint64_t aura_minute = state.updated_world_minute;
        if (const auto* dynamics = player_dynamics_.find(state.player)) {
            aura_minute = std::max(aura_minute, dynamics->updated_world_minute);
        }
        AuraState aura = aura_from_resolution(
            resolution.value(), aura_minute, current_aura == nullptr ? 1 : current_aura->sequence + 1);
        const auto set_aura = staged.set_aura(registry_, player_life_, aura);
        if (!set_aura) return set_aura;
        changes.push_back({
            WorldChangeKind::AuraStateChanged,
            AuraStateChanged{aura_before, aura}});
    }

    const auto committed = commit(std::move(changes));
    if (!committed) return committed;
    theorism_ = std::move(staged);
    return Result<void>::success();
}

Result<HalaluluResolution> VersionedWorld::resolve_player_aura(EntityId player) const {
    return resolve_halalulu(player, theorism_.subclasses_for(player), player_dynamics_.find(player));
}

} // namespace home
