#include "home/guff_home_cartridge.hpp"

#include <cassert>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{1}};

    ZoneCreateInfo zone_info{};
    zone_info.kind = ZoneKind::District;
    zone_info.key = "mission-bay";
    zone_info.display_name = "Mission Bay";
    const auto zone_result = world.create_zone(zone_info);
    assert(zone_result);
    const ZoneId zone = zone_result.value();

    EntityCreateInfo entity_info{};
    entity_info.kind = EntityKind::Player;
    entity_info.archetype = "agnathos";
    entity_info.display_name = "Agnathos";
    const auto entity_result = world.create_entity(entity_info);
    assert(entity_result);
    const EntityId entity = entity_result.value();
    assert(world.place_entity(entity, zone));

    GuffHomeCartridge cartridge{world};
    const auto initial = cartridge.context();
    assert(initial.world == WorldId{1});
    assert(initial.revision == world.revision());
    assert(initial.calendar == world.calendar());
    assert(initial.snapshot.world == world.world());
    assert(initial.snapshot.revision == world.revision());
    assert(initial.snapshot.entities.size() == 1);
    assert(initial.snapshot.placements.size() == 1);

    const auto baseline = world.revision();
    GuffHomeProposal proposal{};
    proposal.id = WorldTransactionId{23};
    proposal.expected_revision = baseline;
    proposal.requester = "planner";
    proposal.operations.push_back(TxUpdateTransform{
        entity,
        Transform{Vec3Mm{1200, 3400, 0}, Millidegrees{0}, Millidegrees{0}, Millidegrees{90000}}
    });

    const auto receipt = cartridge.submit(proposal);
    assert(receipt);
    assert(receipt.value().id == WorldTransactionId{23});
    assert(receipt.value().from_revision == baseline);
    assert(receipt.value().to_revision == world.revision());
    assert(world.revision().value == baseline.value + 1);

    const auto changed = world.entities().find(entity);
    assert(changed);
    assert(changed.value().transform.position == Vec3Mm{1200, 3400, 0});

    const auto deltas = cartridge.deltas_since(baseline);
    assert(deltas);
    assert(deltas.value().size() == 1);
    assert(deltas.value().front().revision == world.revision());

    GuffHomeProposal stale = proposal;
    stale.id = WorldTransactionId{24};
    const auto stale_result = cartridge.submit(stale);
    assert(!stale_result);
    assert(stale_result.error().code == ErrorCode::RevisionConflict);

    GuffHomeProposal anonymous{};
    anonymous.id = WorldTransactionId{25};
    anonymous.expected_revision = world.revision();
    const auto anonymous_result = cartridge.submit(anonymous);
    assert(!anonymous_result);
    assert(anonymous_result.error().code == ErrorCode::InvalidArgument);

    return 0;
}
