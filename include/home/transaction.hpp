#pragma once

#include "home/entity.hpp"
#include "home/revision.hpp"
#include "home/topology.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace home {

struct TxCreateEntity { EntityCreateInfo info{}; };
struct TxRemoveEntity { EntityId id{}; };
struct TxUpdateTransform { EntityId id{}; Transform transform{}; };
struct TxCreateZone { ZoneCreateInfo info{}; };
struct TxSetZoneParent { ZoneId child{}; std::optional<ZoneId> parent{}; };
struct TxConnectZones { ZoneId from{}; ZoneId to{}; bool bidirectional{true}; };
struct TxPlaceEntity { EntityId entity{}; std::optional<ZoneId> zone{}; };

using WorldOperation = std::variant<
    TxCreateEntity,
    TxRemoveEntity,
    TxUpdateTransform,
    TxCreateZone,
    TxSetZoneParent,
    TxConnectZones,
    TxPlaceEntity
>;

struct WorldTransaction final {
    WorldTransactionId id{};
    WorldRevision expected_revision{};
    std::string authority{};
    std::vector<WorldOperation> operations{};
};

struct TransactionReceipt final {
    WorldTransactionId id{};
    WorldRevision from_revision{};
    WorldRevision to_revision{};
    std::vector<EntityId> created_entities{};
    std::vector<ZoneId> created_zones{};
};

} // namespace home
