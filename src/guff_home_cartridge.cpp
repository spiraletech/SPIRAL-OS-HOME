#include "home/guff_home_cartridge.hpp"

#include <algorithm>
#include <cctype>
#include <type_traits>
#include <utility>

namespace home {

namespace {

bool valid_principal(const std::string& value) {
    if (value.empty() || value.size() > 128) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '.' || c == '_' || c == '-';
    });
}

bool has_capability(const std::vector<GuffHomeCapability>& values, GuffHomeCapability capability) {
    return std::find(values.begin(), values.end(), capability) != values.end();
}

void push_unique(std::vector<GuffHomeCapability>& values, GuffHomeCapability capability) {
    if (!has_capability(values, capability)) values.push_back(capability);
}

} // namespace

GuffHomeCapability guff_capability_for(const WorldOperation& operation) {
    return std::visit([](const auto& command) {
        using T = std::decay_t<decltype(command)>;
        if constexpr (std::is_same_v<T, TxCreateEntity>) return GuffHomeCapability::CreateEntity;
        if constexpr (std::is_same_v<T, TxRemoveEntity>) return GuffHomeCapability::RemoveEntity;
        if constexpr (std::is_same_v<T, TxUpdateTransform>) return GuffHomeCapability::UpdateTransform;
        if constexpr (std::is_same_v<T, TxCreateZone>) return GuffHomeCapability::CreateZone;
        if constexpr (std::is_same_v<T, TxSetZoneParent>) return GuffHomeCapability::SetZoneParent;
        if constexpr (std::is_same_v<T, TxConnectZones>) return GuffHomeCapability::ConnectZones;
        return GuffHomeCapability::PlaceEntity;
    }, operation);
}

Result<void> GuffHomeCartridge::validate_grant() const {
    if (!world_) return Result<void>::failure(ErrorCode::InternalError, "GUFF HOME cartridge has no world");
    if (!valid_principal(grant_.principal)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "GUFF HOME grant principal is invalid");
    }
    if (grant_.max_operations_per_proposal == 0 || grant_.max_operations_per_proposal > kGuffHomeAbsoluteOperationLimit) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "GUFF HOME grant operation limit is invalid");
    }
    return Result<void>::success();
}

bool GuffHomeCartridge::allows(GuffHomeCapability capability) const noexcept {
    return has_capability(grant_.capabilities, capability);
}

Result<void> GuffHomeCartridge::validate_proposal(const GuffHomeProposal& proposal) const {
    const auto grant_status = validate_grant();
    if (!grant_status) return grant_status;
    if (!proposal.id.valid()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "GUFF HOME proposal transaction id must be valid");
    }
    if (proposal.principal != grant_.principal) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "GUFF HOME proposal principal does not match cartridge grant");
    }
    if (proposal.rationale.empty() || proposal.rationale.size() > kGuffHomeRationaleLimit) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "GUFF HOME proposal rationale is missing or too large");
    }
    if (proposal.expected_revision != world_->revision()) {
        return Result<void>::failure(ErrorCode::RevisionConflict, "GUFF HOME proposal expected revision does not match canonical revision");
    }
    if (proposal.operations.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "GUFF HOME proposal must contain operations");
    }
    if (proposal.operations.size() > grant_.max_operations_per_proposal) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "GUFF HOME proposal exceeds granted operation limit");
    }
    for (const auto& operation : proposal.operations) {
        if (!allows(guff_capability_for(operation))) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "GUFF HOME proposal contains an ungranted operation capability");
        }
    }
    return Result<void>::success();
}

GuffHomeContext GuffHomeCartridge::context() const {
    GuffHomeContext out{};
    out.world = world_->world();
    out.revision = world_->revision();
    out.world_time = world_->clock().now();
    out.calendar = world_->calendar();
    out.affect = world_->affect();
    out.anchor = world_->anchor();
    out.snapshot = world_->snapshot();
    return out;
}

Result<std::vector<WorldDelta>> GuffHomeCartridge::deltas_since(WorldRevision revision) const {
    return world_->deltas_since(revision);
}

Result<GuffHomeDecision> GuffHomeCartridge::submit(const GuffHomeProposal& proposal) {
    const auto validated = validate_proposal(proposal);
    if (!validated) {
        return Result<GuffHomeDecision>::failure(validated.error().code, validated.error().message);
    }

    WorldTransaction transaction{};
    transaction.id = proposal.id;
    transaction.expected_revision = proposal.expected_revision;
    transaction.authority = "guff.home/" + grant_.principal;
    transaction.operations = proposal.operations;

    auto receipt = world_->execute(transaction);
    if (!receipt) {
        return Result<GuffHomeDecision>::failure(receipt.error().code, receipt.error().message);
    }

    GuffHomeDecision decision{};
    decision.receipt = receipt.value();
    decision.principal = grant_.principal;
    decision.authority = transaction.authority;
    decision.rationale = proposal.rationale;
    for (const auto& operation : proposal.operations) push_unique(decision.exercised_capabilities, guff_capability_for(operation));
    return Result<GuffHomeDecision>::success(std::move(decision));
}

} // namespace home
