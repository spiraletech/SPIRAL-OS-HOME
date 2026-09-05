#pragma once

#include <cstdint>
#include <string>

namespace spiral::world {

// Reference C++ adapter types only. These layouts are NOT part of the wire ABI.
// Canonical identity semantics live in the versioned World State schema.

template <typename Tag>
struct StableId {
    std::string value;

    [[nodiscard]] bool empty() const noexcept { return value.empty(); }
    friend bool operator==(const StableId&, const StableId&) = default;
};

struct WorldIdTag {};
struct EntityIdTag {};
struct ComponentIdTag {};
struct SpatialNodeIdTag {};
struct AuthorityIdTag {};
struct CommandIdTag {};
struct DeltaIdTag {};
struct EventIdTag {};
struct TransactionIdTag {};
struct OriginIdTag {};

using WorldId = StableId<WorldIdTag>;
using EntityId = StableId<EntityIdTag>;
using ComponentId = StableId<ComponentIdTag>;
using SpatialNodeId = StableId<SpatialNodeIdTag>;
using AuthorityId = StableId<AuthorityIdTag>;
using CommandId = StableId<CommandIdTag>;
using DeltaId = StableId<DeltaIdTag>;
using EventId = StableId<EventIdTag>;
using TransactionId = StableId<TransactionIdTag>;
using OriginId = StableId<OriginIdTag>;

using WorldEpoch = std::uint64_t;
using WorldTick = std::uint64_t;
using SequenceNumber = std::uint64_t;
using Revision = std::uint64_t;

} // namespace spiral::world
