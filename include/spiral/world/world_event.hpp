#pragma once

#include <optional>
#include <string>

#include "spiral/world/world_ids.hpp"

namespace spiral::world {

struct WorldEvent {
    EventId id{};
    std::optional<CommandId> cause{};
    TransactionId transaction{};
    AuthorityId authority{};
    WorldId world_id{};
    WorldEpoch epoch{0};
    WorldTick tick{0};
    SequenceNumber sequence{0};
    std::string event_type{};
    std::string payload_json{};
};

} // namespace spiral::world
