#pragma once

#include <optional>
#include <string>

#include "spiral/world/world_ids.hpp"

namespace spiral::world {

struct WorldCommand {
    CommandId id{};
    WorldId world_id{};
    WorldEpoch epoch{0};
    WorldTick issued_at_tick{0};
    AuthorityId requester{};
    std::string command_type{};
    std::string payload_json{};
    std::optional<TransactionId> requested_transaction{};
};

} // namespace spiral::world
