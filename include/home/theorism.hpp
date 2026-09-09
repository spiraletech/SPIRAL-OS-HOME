#pragma once

#include "home/entity_registry.hpp"
#include "home/needs_mood_autonomy.hpp"
#include "home/player_life.hpp"
#include "home/result.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace home {

inline constexpr std::uint32_t kSubclassAffinityMaximum = 1000;
inline constexpr std::int32_t kAuraChargeMinimum = -1000;
inline constexpr std::int32_t kAuraChargeMaximum = 1000;
inline constexpr std::uint32_t kAuraIntensityMaximum = 1000;
inline constexpr std::uint32_t kAuraCoherenceMaximum = 1000;

struct SubclassAffinityState final {
    EntityId player{};
    std::string key{};
    std::uint64_t evidence_points{1};
    std::uint32_t affinity_permille{1};
    std::uint64_t discovered_world_minute{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};

    auto operator<=>(const SubclassAffinityState&) const = default;
};

struct AuraState final {
    EntityId player{};
    std::string dominant_affinity{};
    std::string signature{};
    std::uint32_t intensity_permille{};
    std::int32_t charge_milli{};
    std::uint32_t coherence_permille{kAuraCoherenceMaximum};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};

    auto operator<=>(const AuraState&) const = default;
};

struct HalaluluResolution final {
    EntityId player{};
    std::string dominant_affinity{};
    std::string signature{};
    std::uint32_t intensity_permille{};
    std::int32_t charge_milli{};
    std::uint32_t coherence_permille{kAuraCoherenceMaximum};

    auto operator<=>(const HalaluluResolution&) const = default;
};

struct TheorismPurge final {
    std::vector<SubclassAffinityState> subclasses{};
    std::optional<AuraState> aura{};
};

[[nodiscard]] std::uint32_t subclass_affinity_for_evidence(std::uint64_t evidence_points) noexcept;
[[nodiscard]] Result<void> validate_subclass_affinity_shape(const SubclassAffinityState& state);
[[nodiscard]] Result<void> validate_aura_shape(const AuraState& state);
[[nodiscard]] Result<HalaluluResolution> resolve_halalulu(
    EntityId player,
    const std::vector<SubclassAffinityState>& subclasses,
    const PlayerDynamicsState* dynamics);

class TheorismLedger final {
public:
    Result<void> set_subclass(
        const EntityRegistry& entities,
        const PlayerLifeLedger& life,
        SubclassAffinityState state);
    Result<void> restore_subclass(
        const EntityRegistry& entities,
        const PlayerLifeLedger& life,
        SubclassAffinityState state);

    Result<void> set_aura(
        const EntityRegistry& entities,
        const PlayerLifeLedger& life,
        AuraState state);
    Result<void> restore_aura(
        const EntityRegistry& entities,
        const PlayerLifeLedger& life,
        AuraState state);

    [[nodiscard]] const SubclassAffinityState* find_subclass(EntityId player, std::string_view key) const noexcept;
    [[nodiscard]] std::vector<SubclassAffinityState> subclasses_for(EntityId player) const;
    [[nodiscard]] const AuraState* find_aura(EntityId player) const noexcept;

    [[nodiscard]] TheorismPurge purge_player(EntityId player);
    [[nodiscard]] std::vector<SubclassAffinityState> subclass_snapshot() const;
    [[nodiscard]] std::vector<AuraState> aura_snapshot() const;

private:
    std::vector<SubclassAffinityState> subclasses_{};
    std::vector<AuraState> auras_{};
};

} // namespace home
