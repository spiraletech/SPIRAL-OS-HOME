#include "home/guff_home_cartridge.hpp"

#include <utility>

namespace home {

GuffHomeContext GuffHomeCartridge::context() const {
    GuffHomeContext context{};
    context.world = world_->world();
    context.revision = world_->revision();
    context.calendar = world_->calendar();
    context.snapshot = world_->snapshot();
    return context;
}

Result<std::vector<WorldDelta>> GuffHomeCartridge::deltas_since(WorldRevision revision) const {
    return world_->deltas_since(revision);
}

Result<TransactionReceipt> GuffHomeCartridge::submit(const GuffHomeProposal& proposal) {
    if (proposal.requester.empty()) {
        return Result<TransactionReceipt>::failure(
            ErrorCode::InvalidArgument,
            "GUFF HOME proposal requester must not be empty");
    }

    WorldTransaction transaction{};
    transaction.id = proposal.id;
    transaction.expected_revision = proposal.expected_revision;
    transaction.authority = "guff/" + proposal.requester;
    transaction.operations = proposal.operations;
    return world_->execute(transaction);
}

} // namespace home
