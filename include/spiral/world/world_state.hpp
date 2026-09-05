#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "spiral/world/coordinate_frame.hpp"
#include "spiral/world/spatial_graph.hpp"
#include "spiral/world/world_ids.hpp"

namespace spiral::world {

enum class LifecycleState {
    Declared,
    Active,
    Dormant,
    Destroyed,
};

struct ComponentRecord {
    ComponentId id{};
    std::string type{};
    std::string schema_version{};
    Revision revision{0};
    std::string payload_json{};
};

struct EntityRecord {
    EntityId id{};
    std::string kind{};
    LifecycleState lifecycle{LifecycleState::Declared};
    std::vector<ComponentRecord> components{};
};

enum class AuthorityScopeKind {
    WorldDomain,
    Entity,
    EntityComponent,
    SpatialNode,
};

struct AuthorityAssignment {
    AuthorityScopeKind scope_kind{AuthorityScopeKind::WorldDomain};
    std::string domain{};
    std::string scope_id{};
    std::string component_type{};
    AuthorityId authority{};
    WorldTick effective_tick{0};
};

struct WorldSnapshot {
    std::string schema_version{"0.1"};
    WorldId world_id{};
    WorldEpoch epoch{0};
    WorldTick tick{0};
    std::int64_t world_time_microseconds{0};
    std::uint64_t seed{0};
    CoordinateFrame coordinate_frame{};
    std::vector<AuthorityAssignment> authority_assignments{};
    SpatialGraph spatial_graph{};
    std::vector<EntityRecord> entities{};
};

} // namespace spiral::world
