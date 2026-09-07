#include "home/relationships.hpp"

#include <algorithm>
#include <utility>

namespace home {
namespace {

std::pair<PlayerId, PlayerId> normalize_pair(PlayerId a, PlayerId b) noexcept {
    return a.value() <= b.value() ? std::pair{a, b} : std::pair{b, a};
}

bool valid_kind(RelationshipKind kind) noexcept {
    switch (kind) {
        case RelationshipKind::Acquaintance:
        case RelationshipKind::Friend:
        case RelationshipKind::Family:
        case RelationshipKind::Romantic:
        case RelationshipKind::Rival:
            return true;
    }
    return false;
}

bool valid_affinity(std::int16_t affinity) noexcept {
    return affinity >= kRelationshipAffinityMin && affinity <= kRelationshipAffinityMax;
}

RelationshipState* find_relationship_mutable(std::vector<RelationshipState>& relationships, PlayerId a, PlayerId b) noexcept {
    const auto [first, second] = normalize_pair(a, b);
    auto it = std::find_if(relationships.begin(), relationships.end(), [&](const RelationshipState& state) {
        return state.first == first && state.second == second;
    });
    return it == relationships.end() ? nullptr : &*it;
}

HouseholdState* find_household_mutable(std::vector<HouseholdState>& households, HouseholdId id) noexcept {
    auto it = std::find_if(households.begin(), households.end(), [&](const HouseholdState& state) { return state.id == id; });
    return it == households.end() ? nullptr : &*it;
}

void sort_relationships(std::vector<RelationshipState>& relationships) {
    std::sort(relationships.begin(), relationships.end(), [](const RelationshipState& a, const RelationshipState& b) {
        if (a.first.value() != b.first.value()) return a.first.value() < b.first.value();
        return a.second.value() < b.second.value();
    });
}

void sort_members(std::vector<PlayerId>& members) {
    std::sort(members.begin(), members.end(), [](PlayerId a, PlayerId b) { return a.value() < b.value(); });
}

bool has_duplicate_members(const std::vector<PlayerId>& members) noexcept {
    for (std::size_t i = 1; i < members.size(); ++i) {
        if (members[i - 1] == members[i]) return true;
    }
    return false;
}

} // namespace

Result<void> RelationshipsLedger::create_relationship(RelationshipState state) {
    if (!state.first.valid() || !state.second.valid() || state.first == state.second) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "relationship endpoints must be distinct valid players");
    }
    if (!valid_kind(state.kind) || !valid_affinity(state.affinity)) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "invalid relationship state");
    }
    const auto [first, second] = normalize_pair(state.first, state.second);
    if (find_relationship(first, second)) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "relationship already exists");
    }
    state.first = first;
    state.second = second;
    relationships_.push_back(std::move(state));
    sort_relationships(relationships_);
    return Result<void>::success();
}

Result<void> RelationshipsLedger::set_relationship_kind(PlayerId a, PlayerId b, RelationshipKind kind) {
    if (!valid_kind(kind)) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid relationship kind");
    auto* state = find_relationship_mutable(relationships_, a, b);
    if (!state) return Result<void>::failure(ErrorCode::NotFound, "relationship not found");
    if (state->kind == kind) return Result<void>::failure(ErrorCode::ValidationFailed, "relationship kind did not change");
    state->kind = kind;
    ++state->relationship_revision;
    return Result<void>::success();
}

Result<void> RelationshipsLedger::set_affinity(PlayerId a, PlayerId b, std::int16_t affinity) {
    if (!valid_affinity(affinity)) return Result<void>::failure(ErrorCode::InvalidArgument, "relationship affinity out of range");
    auto* state = find_relationship_mutable(relationships_, a, b);
    if (!state) return Result<void>::failure(ErrorCode::NotFound, "relationship not found");
    if (state->affinity == affinity) return Result<void>::failure(ErrorCode::ValidationFailed, "relationship affinity did not change");
    state->affinity = affinity;
    ++state->relationship_revision;
    return Result<void>::success();
}

Result<void> RelationshipsLedger::remove_relationship(PlayerId a, PlayerId b) {
    const auto [first, second] = normalize_pair(a, b);
    auto it = std::find_if(relationships_.begin(), relationships_.end(), [&](const RelationshipState& state) {
        return state.first == first && state.second == second;
    });
    if (it == relationships_.end()) return Result<void>::failure(ErrorCode::NotFound, "relationship not found");
    relationships_.erase(it);
    return Result<void>::success();
}

const RelationshipState* RelationshipsLedger::find_relationship(PlayerId a, PlayerId b) const noexcept {
    if (!a.valid() || !b.valid() || a == b) return nullptr;
    const auto [first, second] = normalize_pair(a, b);
    auto it = std::find_if(relationships_.begin(), relationships_.end(), [&](const RelationshipState& state) {
        return state.first == first && state.second == second;
    });
    return it == relationships_.end() ? nullptr : &*it;
}

std::vector<RelationshipState> RelationshipsLedger::relationship_snapshot() const { return relationships_; }

Result<HouseholdId> RelationshipsLedger::create_household(std::string name, std::vector<PlayerId> members) {
    if (name.empty()) return Result<HouseholdId>::failure(ErrorCode::InvalidArgument, "household name must not be empty");
    for (const auto player : members) {
        if (!player.valid()) return Result<HouseholdId>::failure(ErrorCode::InvalidArgument, "household members must be valid players");
        if (household_for(player)) return Result<HouseholdId>::failure(ErrorCode::AlreadyExists, "player already belongs to a household");
    }
    sort_members(members);
    if (has_duplicate_members(members)) return Result<HouseholdId>::failure(ErrorCode::InvalidArgument, "household members must be unique");

    const HouseholdId id{next_household_id_++};
    households_.push_back(HouseholdState{id, std::move(name), std::move(members), 0});
    return Result<HouseholdId>::success(id);
}

Result<void> RelationshipsLedger::rename_household(HouseholdId id, std::string name) {
    if (!id.valid() || name.empty()) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid household rename");
    auto* household = find_household_mutable(households_, id);
    if (!household) return Result<void>::failure(ErrorCode::NotFound, "household not found");
    if (household->name == name) return Result<void>::failure(ErrorCode::ValidationFailed, "household name did not change");
    household->name = std::move(name);
    ++household->household_revision;
    return Result<void>::success();
}

Result<void> RelationshipsLedger::add_member(HouseholdId id, PlayerId player) {
    if (!id.valid() || !player.valid()) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid household member");
    auto* household = find_household_mutable(households_, id);
    if (!household) return Result<void>::failure(ErrorCode::NotFound, "household not found");
    if (household_for(player)) return Result<void>::failure(ErrorCode::AlreadyExists, "player already belongs to a household");
    household->members.push_back(player);
    sort_members(household->members);
    ++household->household_revision;
    return Result<void>::success();
}

Result<void> RelationshipsLedger::remove_member(HouseholdId id, PlayerId player) {
    if (!id.valid() || !player.valid()) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid household member");
    auto* household = find_household_mutable(households_, id);
    if (!household) return Result<void>::failure(ErrorCode::NotFound, "household not found");
    auto it = std::find(household->members.begin(), household->members.end(), player);
    if (it == household->members.end()) return Result<void>::failure(ErrorCode::NotFound, "player is not a household member");
    household->members.erase(it);
    ++household->household_revision;
    return Result<void>::success();
}

Result<void> RelationshipsLedger::dissolve_household(HouseholdId id) {
    if (!id.valid()) return Result<void>::failure(ErrorCode::InvalidArgument, "invalid household id");
    auto it = std::find_if(households_.begin(), households_.end(), [&](const HouseholdState& state) { return state.id == id; });
    if (it == households_.end()) return Result<void>::failure(ErrorCode::NotFound, "household not found");
    households_.erase(it);
    return Result<void>::success();
}

const HouseholdState* RelationshipsLedger::find_household(HouseholdId id) const noexcept {
    auto it = std::find_if(households_.begin(), households_.end(), [&](const HouseholdState& state) { return state.id == id; });
    return it == households_.end() ? nullptr : &*it;
}

const HouseholdState* RelationshipsLedger::household_for(PlayerId player) const noexcept {
    if (!player.valid()) return nullptr;
    for (const auto& household : households_) {
        if (std::find(household.members.begin(), household.members.end(), player) != household.members.end()) return &household;
    }
    return nullptr;
}

std::vector<HouseholdState> RelationshipsLedger::household_snapshot() const { return households_; }

} // namespace home
