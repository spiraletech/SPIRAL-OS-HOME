#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace home {

constexpr std::int16_t kRelationshipAffinityMin = -1000;
constexpr std::int16_t kRelationshipAffinityMax = 1000;

enum class RelationshipKind : std::uint8_t {
    Acquaintance = 0,
    Friend,
    Family,
    Romantic,
    Rival
};

struct RelationshipState final {
    PlayerId first{};
    PlayerId second{};
    RelationshipKind kind{RelationshipKind::Acquaintance};
    std::int16_t affinity{0};
    std::uint64_t relationship_revision{};
    auto operator<=>(const RelationshipState&) const = default;
};

struct HouseholdState final {
    HouseholdId id{};
    std::string name{};
    std::vector<PlayerId> members{};
    std::uint64_t household_revision{};
    auto operator<=>(const HouseholdState&) const = default;
};

class RelationshipsLedger final {
public:
    Result<void> create_relationship(RelationshipState state);
    Result<void> set_relationship_kind(PlayerId a, PlayerId b, RelationshipKind kind);
    Result<void> set_affinity(PlayerId a, PlayerId b, std::int16_t affinity);
    Result<void> remove_relationship(PlayerId a, PlayerId b);
    [[nodiscard]] const RelationshipState* find_relationship(PlayerId a, PlayerId b) const noexcept;
    [[nodiscard]] std::vector<RelationshipState> relationship_snapshot() const;

    Result<HouseholdId> create_household(std::string name, std::vector<PlayerId> members = {});
    Result<void> rename_household(HouseholdId id, std::string name);
    Result<void> add_member(HouseholdId id, PlayerId player);
    Result<void> remove_member(HouseholdId id, PlayerId player);
    Result<void> dissolve_household(HouseholdId id);
    [[nodiscard]] const HouseholdState* find_household(HouseholdId id) const noexcept;
    [[nodiscard]] const HouseholdState* household_for(PlayerId player) const noexcept;
    [[nodiscard]] std::vector<HouseholdState> household_snapshot() const;

private:
    std::vector<RelationshipState> relationships_{};
    std::vector<HouseholdState> households_{};
    std::uint64_t next_household_id_{1};
};

} // namespace home
