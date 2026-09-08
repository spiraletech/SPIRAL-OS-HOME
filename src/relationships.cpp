#include "home/relationships.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {
namespace {

bool valid_relationship_kind(RelationshipKind kind) noexcept {
    return static_cast<unsigned>(kind) <= static_cast<unsigned>(RelationshipKind::Dependent);
}

bool valid_affinity(std::int32_t value) noexcept {
    return value >= kRelationshipAffinityMinimum && value <= kRelationshipAffinityMaximum;
}

bool valid_trust(std::int32_t value) noexcept {
    return value >= kRelationshipTrustMinimum && value <= kRelationshipTrustMaximum;
}

bool valid_player_endpoint(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    EntityId entity) noexcept {
    const EntityRecord* record = entities.find(entity);
    return record != nullptr && record->kind == EntityKind::Avatar && life.find(entity) != nullptr;
}

void sort_members(std::vector<EntityId>& members) {
    std::sort(members.begin(), members.end(), [](EntityId a, EntityId b) { return a.value() < b.value(); });
}

bool has_duplicate_members(const std::vector<EntityId>& members) noexcept {
    for (std::size_t i = 1; i < members.size(); ++i) {
        if (members[i - 1] == members[i]) return true;
    }
    return false;
}

} // namespace

Result<void> validate_relationship_state(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    const RelationshipState& state) {
    if (!state.from.valid() || !state.to.valid() || state.from == state.to) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "relationship endpoints must be distinct valid entities");
    }
    if (!valid_player_endpoint(entities, life, state.from) || !valid_player_endpoint(entities, life, state.to)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "relationship endpoints require canonical avatar life state");
    }
    if (!valid_relationship_kind(state.kind)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "relationship kind is invalid");
    }
    if (!valid_affinity(state.affinity) || !valid_trust(state.trust)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "relationship affinity or trust is outside canonical range");
    }
    if (state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "relationship sequence must be nonzero");
    }
    return Result<void>::success();
}

Result<void> validate_household_state(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    const PlayerLifeLedger& life,
    const HouseholdState& state) {
    if (!state.id.valid()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "household id must be valid");
    }
    if (state.name.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "household name must not be empty");
    }
    if (state.members.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "household must contain at least one member");
    }
    if (state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "household sequence must be nonzero");
    }
    if (state.home_zone.has_value() && !topology.contains(*state.home_zone)) {
        return Result<void>::failure(ErrorCode::NotFound, "household home zone does not exist");
    }

    std::vector<EntityId> members = state.members;
    sort_members(members);
    if (has_duplicate_members(members)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "household members must be unique");
    }
    for (const EntityId member : members) {
        if (!valid_player_endpoint(entities, life, member)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "household members require canonical avatar life state");
        }
    }
    return Result<void>::success();
}

Result<void> RelationshipsLedger::set_relationship(
    const EntityRegistry& entities,
    const PlayerLifeLedger& life,
    RelationshipState state) {
    const auto valid = validate_relationship_state(entities, life, state);
    if (!valid) return valid;

    const auto it = std::lower_bound(relationships_.begin(), relationships_.end(), state,
        [](const RelationshipState& a, const RelationshipState& b) {
            if (a.from != b.from) return a.from < b.from;
            return a.to < b.to;
        });

    if (it != relationships_.end() && it->from == state.from && it->to == state.to) {
        if (state.sequence <= it->sequence) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "relationship sequence is stale");
        }
        if (state.updated_world_minute < it->updated_world_minute) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "relationship world minute regressed");
        }
        *it = std::move(state);
        return Result<void>::success();
    }

    if (state.sequence != 1) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "new relationship must begin at sequence 1");
    }
    relationships_.insert(it, std::move(state));
    return Result<void>::success();
}

Result<RelationshipState> RelationshipsLedger::remove_relationship(EntityId from, EntityId to) {
    const auto it = std::lower_bound(relationships_.begin(), relationships_.end(), std::pair{from, to},
        [](const RelationshipState& state, const std::pair<EntityId, EntityId>& key) {
            if (state.from != key.first) return state.from < key.first;
            return state.to < key.second;
        });
    if (it == relationships_.end() || it->from != from || it->to != to) {
        return Result<RelationshipState>::failure(ErrorCode::NotFound, "relationship not found");
    }
    RelationshipState removed = *it;
    relationships_.erase(it);
    return Result<RelationshipState>::success(std::move(removed));
}

Result<HouseholdId> RelationshipsLedger::allocate_household_id() {
    if (next_household_value_ == 0 || next_household_value_ == std::numeric_limits<std::uint64_t>::max()) {
        return Result<HouseholdId>::failure(ErrorCode::Overflow, "household id space exhausted");
    }
    return Result<HouseholdId>::success(HouseholdId{next_household_value_++});
}

void RelationshipsLedger::advance_allocator_past(HouseholdId id) noexcept {
    if (id.value() >= next_household_value_ && id.value() != std::numeric_limits<std::uint64_t>::max()) {
        next_household_value_ = id.value() + 1;
    }
}

Result<HouseholdId> RelationshipsLedger::create_household(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    const PlayerLifeLedger& life,
    HouseholdCreateInfo info) {
    const auto allocated = allocate_household_id();
    if (!allocated) return allocated;

    HouseholdState state{};
    state.id = allocated.value();
    state.name = std::move(info.name);
    state.members = std::move(info.members);
    state.home_zone = info.home_zone;
    state.updated_world_minute = info.updated_world_minute;
    state.sequence = 1;
    sort_members(state.members);

    const auto valid = validate_household_state(entities, topology, life, state);
    if (!valid) {
        --next_household_value_;
        return Result<HouseholdId>::failure(valid.error().code, valid.error().message);
    }
    for (const EntityId member : state.members) {
        if (household_for(member) != nullptr) {
            --next_household_value_;
            return Result<HouseholdId>::failure(ErrorCode::AlreadyExists, "player already belongs to a household");
        }
    }
    households_.push_back(std::move(state));
    std::sort(households_.begin(), households_.end(), [](const HouseholdState& a, const HouseholdState& b) {
        return a.id < b.id;
    });
    return allocated;
}

Result<void> RelationshipsLedger::set_household(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    const PlayerLifeLedger& life,
    HouseholdState state) {
    sort_members(state.members);
    const auto valid = validate_household_state(entities, topology, life, state);
    if (!valid) return valid;

    const auto it = std::lower_bound(households_.begin(), households_.end(), state.id,
        [](const HouseholdState& household, HouseholdId id) { return household.id < id; });

    for (const EntityId member : state.members) {
        const HouseholdState* existing = household_for(member);
        if (existing != nullptr && existing->id != state.id) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "player already belongs to another household");
        }
    }

    if (it != households_.end() && it->id == state.id) {
        if (state.sequence <= it->sequence) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "household sequence is stale");
        }
        if (state.updated_world_minute < it->updated_world_minute) {
            return Result<void>::failure(ErrorCode::RevisionConflict, "household world minute regressed");
        }
        *it = std::move(state);
        return Result<void>::success();
    }

    if (state.sequence != 1) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "restored household must begin at sequence 1 when not already present");
    }
    households_.insert(it, state);
    advance_allocator_past(state.id);
    return Result<void>::success();
}

Result<HouseholdState> RelationshipsLedger::dissolve_household(HouseholdId id) {
    const auto it = std::lower_bound(households_.begin(), households_.end(), id,
        [](const HouseholdState& household, HouseholdId value) { return household.id < value; });
    if (it == households_.end() || it->id != id) {
        return Result<HouseholdState>::failure(ErrorCode::NotFound, "household not found");
    }
    HouseholdState removed = *it;
    households_.erase(it);
    return Result<HouseholdState>::success(std::move(removed));
}

Result<SocialPurgeResult> RelationshipsLedger::purge_entity(EntityId entity) {
    if (!entity.valid()) {
        return Result<SocialPurgeResult>::failure(ErrorCode::InvalidArgument, "purged entity id must be valid");
    }

    SocialPurgeResult result{};
    for (auto it = relationships_.begin(); it != relationships_.end();) {
        if (it->from == entity || it->to == entity) {
            result.removed_relationships.push_back(*it);
            it = relationships_.erase(it);
        } else {
            ++it;
        }
    }

    auto household_it = std::find_if(households_.begin(), households_.end(), [&](const HouseholdState& household) {
        return std::find(household.members.begin(), household.members.end(), entity) != household.members.end();
    });
    if (household_it != households_.end()) {
        result.household_before = *household_it;
        household_it->members.erase(
            std::remove(household_it->members.begin(), household_it->members.end(), entity),
            household_it->members.end());
        if (household_it->members.empty()) {
            households_.erase(household_it);
            result.household_after = std::nullopt;
        } else {
            if (household_it->sequence == std::numeric_limits<std::uint64_t>::max()) {
                return Result<SocialPurgeResult>::failure(ErrorCode::Overflow, "household sequence space exhausted");
            }
            ++household_it->sequence;
            result.household_after = *household_it;
        }
    }
    return Result<SocialPurgeResult>::success(std::move(result));
}

const RelationshipState* RelationshipsLedger::find_relationship(EntityId from, EntityId to) const noexcept {
    const auto it = std::lower_bound(relationships_.begin(), relationships_.end(), std::pair{from, to},
        [](const RelationshipState& state, const std::pair<EntityId, EntityId>& key) {
            if (state.from != key.first) return state.from < key.first;
            return state.to < key.second;
        });
    return it != relationships_.end() && it->from == from && it->to == to ? &*it : nullptr;
}

const HouseholdState* RelationshipsLedger::find_household(HouseholdId id) const noexcept {
    const auto it = std::lower_bound(households_.begin(), households_.end(), id,
        [](const HouseholdState& household, HouseholdId value) { return household.id < value; });
    return it != households_.end() && it->id == id ? &*it : nullptr;
}

const HouseholdState* RelationshipsLedger::household_for(EntityId member) const noexcept {
    if (!member.valid()) return nullptr;
    for (const auto& household : households_) {
        if (std::find(household.members.begin(), household.members.end(), member) != household.members.end()) {
            return &household;
        }
    }
    return nullptr;
}

std::vector<RelationshipState> RelationshipsLedger::relationship_snapshot() const {
    return relationships_;
}

std::vector<HouseholdState> RelationshipsLedger::household_snapshot() const {
    return households_;
}

} // namespace home
