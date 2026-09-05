#pragma once

#include <optional>
#include <vector>

#include "spiral/world/world_ids.hpp"

namespace spiral::world {

struct WorldTransaction {
    TransactionId id{};
    WorldId world_id{};
    WorldEpoch epoch{0};
    WorldTick base_tick{0};
    WorldTick commit_tick{0};
    SequenceNumber first_sequence{0};
    SequenceNumber last_sequence{0};
    AuthorityId committer{};
    std::optional<CommandId> cause{};
    std::vector<DeltaId> deltas{};
    std::vector<EventId> events{};
};

} // namespace spiral::world
