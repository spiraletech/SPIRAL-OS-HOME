#include "home/guff_home_cartridge.hpp"

#include <cassert>
#include <string>

template <typename T>
concept AcceptsRawWorldTransaction = requires(T& cartridge, const home::WorldTransaction& tx) { cartridge.submit(tx); };

int main() {
    using namespace home;

    static_assert(!AcceptsRawWorldTransaction<GuffHomeCartridge>);

    VersionedWorld world{WorldId{23}};

    ZoneCreateInfo district_info{};
    district_info.kind = ZoneKind::District;
    district_info.key = "mission_bay";
    district_info.display_name = "Mission Bay";
    const auto district = world.create_zone(district_info);
    assert(district.ok());

    ZoneCreateInfo pier_info{};
    pier_info.kind = ZoneKind::Parcel;
    pier_info.key = "pier";
    pier_info.display_name = "Pier";
    pier_info.parent = district.value();
    const auto pier = world.create_zone(pier_info);
    assert(pier.ok());

    EntityCreateInfo avatar_info{};
    avatar_info.kind = EntityKind::Avatar;
    avatar_info.archetype = "agnathos";
    avatar_info.display_name = "Agnathos";
    const auto avatar = world.create_entity(avatar_info);
    assert(avatar.ok());
    assert(world.place_entity(avatar.value(), district.value()).ok());

    GuffHomeGrant movement_grant{};
    movement_grant.principal = "planner.alpha";
    movement_grant.capabilities = {GuffHomeCapability::UpdateTransform, GuffHomeCapability::PlaceEntity};
    movement_grant.max_operations_per_proposal = 2;
    GuffHomeCartridge cartridge{world, movement_grant};

    // Context is a read-only, revision-stamped projection of canonical HOME truth.
    const WorldRevision before_context = world.revision();
    const std::size_t history_before_context = world.history().size();
    const GuffHomeContext context = cartridge.context();
    assert(context.projection_version == kGuffHomeProjectionVersion);
    assert(context.world == world.world());
    assert(context.revision == world.revision());
    assert(context.world_time == world.clock().now());
    assert(context.calendar == world.calendar());
    assert(context.affect == world.affect());
    assert(context.anchor == world.anchor());
    assert(context.snapshot.world == world.world());
    assert(context.snapshot.revision == world.revision());
    assert(world.revision() == before_context);
    assert(world.history().size() == history_before_context);

    // One governed proposal may atomically combine only explicitly granted typed operations.
    GuffHomeProposal move{};
    move.id = WorldTransactionId{2301};
    move.expected_revision = world.revision();
    move.principal = "planner.alpha";
    move.rationale = "Move the avatar onto the pier after the navigation planner selected that destination.";
    Transform moved_transform{};
    moved_transform.position = Vec3Mm{1200, 3400, 0};
    moved_transform.rotation = EulerMilliDegrees{0, 90000, 0};
    move.operations.push_back(TxUpdateTransform{avatar.value(), moved_transform});
    move.operations.push_back(TxPlaceEntity{avatar.value(), pier.value()});

    const WorldRevision before_move = world.revision();
    const auto decision = cartridge.submit(move);
    assert(decision.ok());
    assert(decision.value().receipt.id == move.id);
    assert(decision.value().receipt.from_revision == before_move);
    assert(decision.value().receipt.to_revision == world.revision());
    assert(world.revision() == before_move.next().value());
    assert(decision.value().principal == "planner.alpha");
    assert(decision.value().authority == "guff.home/planner.alpha");
    assert(decision.value().rationale == move.rationale);
    assert(decision.value().exercised_capabilities.size() == 2);
    assert(world.entities().find(avatar.value())->transform == moved_transform);
    assert(world.topology().zone_of(avatar.value()) == pier.value());

    const auto deltas = cartridge.deltas_since(before_move);
    assert(deltas.ok());
    assert(deltas.value().size() == 1);
    assert(deltas.value().front().from_revision() == before_move);
    assert(deltas.value().front().to_revision() == world.revision());

    // A stale reasoning result cannot mutate newer HOME truth.
    const WorldRevision after_move = world.revision();
    GuffHomeProposal stale = move;
    stale.id = WorldTransactionId{2302};
    stale.expected_revision = before_move;
    stale.operations = {TxPlaceEntity{avatar.value(), district.value()}};
    const auto stale_result = cartridge.submit(stale);
    assert(!stale_result.ok());
    assert(stale_result.error().code == ErrorCode::RevisionConflict);
    assert(world.revision() == after_move);
    assert(world.topology().zone_of(avatar.value()) == pier.value());

    // Principal identity is bound to the immutable cartridge grant.
    GuffHomeProposal impersonation{};
    impersonation.id = WorldTransactionId{2303};
    impersonation.expected_revision = world.revision();
    impersonation.principal = "planner.beta";
    impersonation.rationale = "Attempt an operation under another principal.";
    impersonation.operations = {TxPlaceEntity{avatar.value(), district.value()}};
    const auto impersonation_result = cartridge.submit(impersonation);
    assert(!impersonation_result.ok());
    assert(world.revision() == after_move);

    // Ungranted destructive capability is rejected before HOME execution.
    GuffHomeProposal removal{};
    removal.id = WorldTransactionId{2304};
    removal.expected_revision = world.revision();
    removal.principal = movement_grant.principal;
    removal.rationale = "Attempt to remove an avatar without a removal grant.";
    removal.operations = {TxRemoveEntity{avatar.value()}};
    const auto removal_result = cartridge.submit(removal);
    assert(!removal_result.ok());
    assert(removal_result.error().code == ErrorCode::ValidationFailed);
    assert(world.entities().contains(avatar.value()));
    assert(world.revision() == after_move);

    // Mixed authorized + unauthorized batches fail preflight with no partial mutation.
    GuffHomeProposal mixed{};
    mixed.id = WorldTransactionId{2305};
    mixed.expected_revision = world.revision();
    mixed.principal = movement_grant.principal;
    mixed.rationale = "This batch must fail because topology creation is not granted.";
    Transform forbidden_partial = moved_transform;
    forbidden_partial.position = Vec3Mm{9999, 9999, 9999};
    ZoneCreateInfo forbidden_zone{};
    forbidden_zone.kind = ZoneKind::Room;
    forbidden_zone.key = "forbidden";
    forbidden_zone.display_name = "Forbidden";
    mixed.operations = {TxUpdateTransform{avatar.value(), forbidden_partial}, TxCreateZone{forbidden_zone}};
    const auto mixed_result = cartridge.submit(mixed);
    assert(!mixed_result.ok());
    assert(world.entities().find(avatar.value())->transform == moved_transform);
    assert(world.topology().snapshot_zones().size() == 2);
    assert(world.revision() == after_move);

    // Proposal bounds and rationale are governance requirements, not model suggestions.
    GuffHomeProposal too_many{};
    too_many.id = WorldTransactionId{2306};
    too_many.expected_revision = world.revision();
    too_many.principal = movement_grant.principal;
    too_many.rationale = "Too many operations.";
    too_many.operations = {
        TxUpdateTransform{avatar.value(), forbidden_partial},
        TxPlaceEntity{avatar.value(), district.value()},
        TxPlaceEntity{avatar.value(), pier.value()}
    };
    assert(!cartridge.submit(too_many).ok());
    assert(world.revision() == after_move);

    GuffHomeProposal no_reason{};
    no_reason.id = WorldTransactionId{2307};
    no_reason.expected_revision = world.revision();
    no_reason.principal = movement_grant.principal;
    no_reason.operations = {TxPlaceEntity{avatar.value(), district.value()}};
    assert(!cartridge.submit(no_reason).ok());
    assert(world.revision() == after_move);

    // Invalid grant identity cannot be used to mint a HOME authority string.
    GuffHomeGrant invalid_grant{};
    invalid_grant.principal = "planner/root";
    invalid_grant.capabilities = {GuffHomeCapability::UpdateTransform};
    GuffHomeCartridge invalid_cartridge{world, invalid_grant};
    GuffHomeProposal invalid_identity{};
    invalid_identity.id = WorldTransactionId{2308};
    invalid_identity.expected_revision = world.revision();
    invalid_identity.principal = invalid_grant.principal;
    invalid_identity.rationale = "Invalid principal token.";
    invalid_identity.operations = {TxUpdateTransform{avatar.value(), forbidden_partial}};
    assert(!invalid_cartridge.submit(invalid_identity).ok());
    assert(world.revision() == after_move);

    // The cartridge is runtime governance transport; canonical snapshot format remains v12.
    const auto encoded = encode_snapshot(world.snapshot());
    assert(encoded.ok());
    assert(encoded.value().rfind("HOME_SNAPSHOT 12", 0) == 0);

    return 0;
}
