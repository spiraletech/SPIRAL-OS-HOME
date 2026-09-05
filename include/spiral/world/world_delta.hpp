#pragma once

#include <optional>
#include <string>
#include <vector>

#include "spiral/world/world_ids.hpp"

namespace spiral::world {

enum class ComponentOperationKind {
    Add,
    Replace,
    Remove,
};

struct ComponentChange {
    EntityId entity_id{};
    std::string component_type{};
    Revision previous_revision{0};
    Revision new_revision{0};
    ComponentOperationKind operation{ComponentOperationKind::Replace};
    std::string payload_json{};
};

struct WorldDelta {
    DeltaId id{};
    std::optional<CommandId> cause{};
    TransactionId transaction{};
    AuthorityId authority{};
    WorldId world_id{};
    WorldEpoch epoch{0};
    WorldTick base_tick{0};
    WorldTick commit_tick{0};
    SequenceNumber sequence{0};
    std::vector<ComponentChange> changes{};
};

} // namespace spiral::world
