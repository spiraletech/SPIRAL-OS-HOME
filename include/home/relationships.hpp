#pragma once

#include "home/entity_registry.hpp"
#include "home/ids.hpp"
#include "home/player_life.hpp"
#include "home/result.hpp"
#include "home/topology.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

inline constexpr std::int32_t kRelationshipAffinityMinimum = -10'000;
inline constexpr std::int32_t kRelationshipAffinityMaximum = 10'000;
inline constexpr std::int32_t kRelationshipTrustMinimum = 0;
inline constexpr std::int32_t kRelationshipTrustMaximum = 10'000;

enum class RelationshipKind : std::uint8_t {
    Acquaintance = 0,
    Friend,
    Family,
    Romantic,
    Rival,
    Guardian,
    Dependent
};

struct RelationshipState final {
    EntityId from{};
    EntityId to{};
    RelationshipKind kind{RelationshipKind::Acquaintance};
    std::int32_t affinity{};
    std::int32_t trust{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};

    auto operator<=>(const RelationshipState&) const = default;
};

struct HouseholdState final {
    HouseholdId id{};
    std::string name{};
    std::vector<EntityId> members{};
    std::optional<ZoneId> home_zone{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};

    auto operator<=>(const HouseholdState&) const = default;
};

struct HouseholdCreateInfo final {
    std::string name{};
    std::vector<EntityId> members{};
    std::optional<ZoneId> home_zone{};
    std::uint64_t updated_world_minute{};
};

struct SocialPurgeResult final {
    std::vector<RelationshipState> removed_relationships{};
    std::optional<HouseholdState> household_before{};
    std::optional<HouseholdState> household_after{};
};

Result<void> validate_relationship_state(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    const RelationshipState& state);

Result<void> validate_household_state(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    const PlayerLifeLedger& life,
    const HouseholdState& state);

class RelationshipsLedger final {
public:
    Result<void> set_relationship(
        const EntityRegistry& entities,
        const PlayerLifeLedger& life,
        RelationshipState state);
    Result<RelationshipState> remove_relationship(EntityId from, EntityId to);

    Result<HouseholdId> create_household(
        const EntityRegistry& entities,
        const TopologyRegistry& topology,
        const PlayerLifeLedger& life,
        HouseholdCreateInfo info);
    Result<void> set_household(
        const EntityRegistry& entities,
        const TopologyRegistry& topology,
        const PlayerLifeLedger& life,
        HouseholdState state);
    Result<HouseholdState> dissolve_household(HouseholdId id);
    Result<SocialPurgeResult> purge_entity(EntityId entity);

    [[nodiscard]] const RelationshipState* find_relationship(EntityId from, EntityId to) const noexcept;
    [[nodiscard]] const HouseholdState* find_household(HouseholdId id) const noexcept;
    [[nodiscard]] const HouseholdState* household_for(EntityId member) const noexcept;
    [[nodiscard]] std::vector<RelationshipState> relationship_snapshot() const;
    [[nodiscard]] std::vector<HouseholdState> household_snapshot() const;

private:
    [[nodiscard]] Result<HouseholdId> allocate_household_id();
    void advance_allocator_past(HouseholdId id) noexcept;

    std::vector<RelationshipState> relationships_{};
    std::vector<HouseholdState> households_{};
    std::uint64_t next_household_value_{1};
};

} // namespace home
