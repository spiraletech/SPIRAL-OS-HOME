#pragma once
#include "home/snapshot.hpp"
#include "home/transaction.hpp"
#include "home/versioned_world.hpp"
#include "home/world_delta.hpp"
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
namespace home {
inline constexpr std::uint32_t kGuffHomeProjectionVersion = 1;
inline constexpr std::size_t kGuffHomeAbsoluteOperationLimit = 64;
inline constexpr std::size_t kGuffHomeRationaleLimit = 1024;
enum class GuffHomeCapability : std::uint8_t { CreateEntity = 0, RemoveEntity, UpdateTransform, CreateZone, SetZoneParent, ConnectZones, PlaceEntity };
struct GuffHomeGrant final { std::string principal{}; std::vector<GuffHomeCapability> capabilities{}; std::size_t max_operations_per_proposal{8}; auto operator<=>(const GuffHomeGrant&) const = default; };
struct GuffHomeContext final { std::uint32_t projection_version{kGuffHomeProjectionVersion}; WorldId world{}; WorldRevision revision{}; WorldTime world_time{}; CalendarState calendar{}; WorldAffectState affect{}; WorldAnchorState anchor{}; WorldSnapshot snapshot{}; };
struct GuffHomeProposal final { WorldTransactionId id{}; WorldRevision expected_revision{}; std::string principal{}; std::string rationale{}; std::vector<WorldOperation> operations{}; };
struct GuffHomeDecision final { TransactionReceipt receipt{}; std::string principal{}; std::string authority{}; std::string rationale{}; std::vector<GuffHomeCapability> exercised_capabilities{}; };
[[nodiscard]] GuffHomeCapability guff_capability_for(const WorldOperation& operation);
class GuffHomeCartridge final {
public:
 GuffHomeCartridge(VersionedWorld& world, GuffHomeGrant grant) : world_(&world), grant_(std::move(grant)) {}
 [[nodiscard]] const GuffHomeGrant& grant() const noexcept { return grant_; }
 [[nodiscard]] GuffHomeContext context() const;
 [[nodiscard]] Result<std::vector<WorldDelta>> deltas_since(WorldRevision revision) const;
 [[nodiscard]] Result<GuffHomeDecision> submit(const GuffHomeProposal& proposal);
private:
 [[nodiscard]] Result<void> validate_grant() const;
 [[nodiscard]] Result<void> validate_proposal(const GuffHomeProposal& proposal) const;
 [[nodiscard]] bool allows(GuffHomeCapability capability) const noexcept;
 VersionedWorld* world_{};
 GuffHomeGrant grant_{};
};
} // namespace home
