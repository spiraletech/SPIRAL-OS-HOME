#pragma once

#include "home/calendar.hpp"
#include "home/snapshot.hpp"
#include "home/transaction.hpp"
#include "home/versioned_world.hpp"
#include "home/world_delta.hpp"

#include <string>
#include <vector>

namespace home {

struct GuffHomeContext final {
    WorldId world{};
    WorldRevision revision{};
    CalendarState calendar{};
    WorldSnapshot snapshot{};
};

struct GuffHomeProposal final {
    WorldTransactionId id{};
    WorldRevision expected_revision{};
    std::string requester{};
    std::vector<WorldOperation> operations{};
};

class GuffHomeCartridge final {
public:
    explicit GuffHomeCartridge(VersionedWorld& world) noexcept : world_(&world) {}

    [[nodiscard]] GuffHomeContext context() const;
    [[nodiscard]] Result<std::vector<WorldDelta>> deltas_since(WorldRevision revision) const;
    Result<TransactionReceipt> submit(const GuffHomeProposal& proposal);

private:
    VersionedWorld* world_{};
};

} // namespace home
