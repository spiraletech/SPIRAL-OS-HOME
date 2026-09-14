#include "home/home_v01_vertical_slice.hpp"
#include "home/snapshot.hpp"

#include <cassert>
#include <utility>

int main() {
    using namespace home;

    const auto invalid = make_home_v01_vertical_slice(WorldId{});
    assert(!invalid);
    assert(invalid.error().code == ErrorCode::InvalidArgument);

    auto built = make_home_v01_vertical_slice(WorldId{25});
    assert(built);
    HomeV01VerticalSlice slice = std::move(built).value();

    assert(slice.world().world() == WorldId{25});
    assert(slice.world().topology().size() == 19);
    assert(slice.world().topology().zone_of(slice.player) == slice.package.zones.crown_point);

    const auto* life = slice.world().player_life().find(slice.player);
    const auto* dynamics = slice.world().player_dynamics().find(slice.player);
    const auto* aura = slice.world().theorism().find_aura(slice.player);
    const auto* skateboard = slice.world().inventory().find(slice.skateboard);
    assert(life && life->stage == LifeStage::Adult && life->presence == LifePresence::Present);
    assert(life->home_zone == slice.package.zones.crown_point);
    assert(dynamics && dynamics->mood.band == MoodBand::Positive);
    assert(aura && aura->dominant_affinity == "street-skater");
    assert(skateboard && skateboard->owner == slice.player && skateboard->kind == ItemKind::Equipment);

    const auto frame0_result = slice.project();
    assert(frame0_result);
    const HomeV01SynchronizedFrame frame0 = frame0_result.value();
    assert(frame0.world == WorldId{25});
    assert(frame0.revision == slice.world().revision());
    assert(frame0.hakui.revision == frame0.revision);
    assert(frame0.inokui.revision == frame0.revision);
    assert(frame0.entrokoi.revision == frame0.revision);
    assert(frame0.xenon.revision == frame0.revision);
    assert(frame0.guff.revision == frame0.revision);
    assert(frame0.entrokoi.observer.entity == slice.player);
    assert(frame0.entrokoi.observer.zone == slice.package.zones.crown_point);
    assert(frame0.entrokoi.observer.aura.has_value());

    bool hakui_saw_player = false;
    for (const auto& body : frame0.hakui.bodies) {
        if (body.entity == slice.player) {
            hakui_saw_player = true;
            assert(body.zone == slice.package.zones.crown_point);
            assert(body.life_stage == LifeStage::Adult);
        }
    }
    assert(hakui_saw_player);

    bool inokui_saw_player = false;
    bool inokui_saw_skateboard = false;
    for (const auto& entity : frame0.inokui.entities) {
        if (entity.entity == slice.player) {
            inokui_saw_player = true;
            assert(entity.zone == slice.package.zones.crown_point);
            assert(entity.mood.has_value());
            assert(entity.aura.has_value());
        }
    }
    for (const auto& item : frame0.inokui.items) {
        if (item.item == slice.skateboard) {
            inokui_saw_skateboard = true;
            assert(item.owner == slice.player);
        }
    }
    assert(inokui_saw_player && inokui_saw_skateboard);

    bool xenon_saw_player = false;
    for (const auto& entity : frame0.xenon.entities) {
        if (entity.entity == slice.player) {
            xenon_saw_player = true;
            assert(entity.zone == slice.package.zones.crown_point);
            assert(entity.aura.has_value());
        }
    }
    assert(xenon_saw_player);

    // HAKUI owns embodied movement. One skate tick updates transform + zone atomically.
    const WorldRevision before_hakui = slice.world().revision();
    const Transform fanuel_transform{
        Vec3Mm{12'000, 4'000, 0},
        EulerMilliDegrees{0, 90'000, 0}
    };
    HakuiConsequenceBatch skate_tick{};
    skate_tick.id = WorldTransactionId{2501};
    skate_tick.expected_revision = frame0.revision;
    skate_tick.authority = "hakui.skate";
    skate_tick.consequences.push_back(HakuiTransformConsequence{slice.player, fanuel_transform});
    skate_tick.consequences.push_back(HakuiZoneConsequence{slice.player, slice.package.zones.fanuel});
    const auto hakui_receipt = slice.apply_hakui(skate_tick);
    assert(hakui_receipt);
    assert(hakui_receipt.value().from_revision == before_hakui);
    assert(hakui_receipt.value().to_revision == slice.world().revision());
    assert(slice.world().revision().value() == before_hakui.value() + 1);
    assert(slice.world().topology().zone_of(slice.player) == slice.package.zones.fanuel);
    assert(slice.world().entities().find(slice.player)->transform == fanuel_transform);

    // The old frame is now stale and cannot be replayed into canonical HOME.
    HakuiConsequenceBatch stale = skate_tick;
    stale.id = WorldTransactionId{2502};
    const auto stale_result = slice.apply_hakui(stale);
    assert(!stale_result);
    assert(stale_result.error().code == ErrorCode::RevisionConflict);

    const auto frame1_result = slice.project();
    assert(frame1_result);
    const HomeV01SynchronizedFrame frame1 = frame1_result.value();
    assert(frame1.revision == slice.world().revision());
    assert(frame1.entrokoi.observer.zone == slice.package.zones.fanuel);
    assert(frame1.entrokoi.observer.transform == fanuel_transform);

    // GUFF proposes a bounded world-authoring adjustment; HOME remains the committer.
    const Transform boundary_transform{
        Vec3Mm{-25'000, 0, 0},
        EulerMilliDegrees{0, 0, 0}
    };
    GuffHomeProposal proposal{};
    proposal.id = WorldTransactionId{2503};
    proposal.expected_revision = frame1.revision;
    proposal.principal = slice.guff_grant.principal;
    proposal.rationale = "Align the Superbloom boundary landmark with the locked east-edge semantic boundary.";
    proposal.operations.push_back(TxUpdateTransform{
        slice.package.landmarks.superbloom_boundary,
        boundary_transform
    });
    const auto guff_decision = slice.submit_guff(proposal);
    assert(guff_decision);
    assert(guff_decision.value().authority == "guff.home/home-v01-planner");
    assert(guff_decision.value().receipt.to_revision == slice.world().revision());
    assert(slice.world().entities().find(slice.package.landmarks.superbloom_boundary)->transform == boundary_transform);

    const auto frame2_result = slice.project();
    assert(frame2_result);
    const HomeV01SynchronizedFrame frame2 = frame2_result.value();
    assert(frame2.revision == slice.world().revision());
    assert(frame2.entrokoi.observer.zone == slice.package.zones.fanuel);

    bool inokui_saw_boundary_transform = false;
    bool xenon_saw_boundary_transform = false;
    for (const auto& entity : frame2.inokui.entities) {
        if (entity.entity == slice.package.landmarks.superbloom_boundary) {
            inokui_saw_boundary_transform = true;
            assert(entity.transform == boundary_transform);
        }
    }
    for (const auto& entity : frame2.xenon.entities) {
        if (entity.entity == slice.package.landmarks.superbloom_boundary) {
            xenon_saw_boundary_transform = true;
            assert(entity.transform == boundary_transform);
        }
    }
    assert(inokui_saw_boundary_transform && xenon_saw_boundary_transform);

    // Destructive GUFF behavior is outside the vertical-slice capability grant.
    const WorldRevision before_denied = slice.world().revision();
    GuffHomeProposal denied{};
    denied.id = WorldTransactionId{2504};
    denied.expected_revision = before_denied;
    denied.principal = slice.guff_grant.principal;
    denied.rationale = "This destructive proposal must be denied by the cartridge capability boundary.";
    denied.operations.push_back(TxRemoveEntity{slice.player});
    const auto denied_result = slice.submit_guff(denied);
    assert(!denied_result);
    assert(denied_result.error().code == ErrorCode::ValidationFailed);
    assert(slice.world().revision() == before_denied);
    assert(slice.world().entities().contains(slice.player));

    // Persist/restore the entire integrated truth, then regenerate every engine view.
    const auto encoded = encode_snapshot(slice.world().snapshot());
    assert(encoded);
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded);
    auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result);
    VersionedWorld restored = std::move(restored_result).value();
    assert(restored.revision() == slice.world().revision());

    const auto restored_frame_result = project_home_v01_frame(restored, slice.player, slice.guff_grant);
    assert(restored_frame_result);
    const HomeV01SynchronizedFrame restored_frame = restored_frame_result.value();
    assert(restored_frame.hakui == frame2.hakui);
    assert(restored_frame.inokui == frame2.inokui);
    assert(restored_frame.entrokoi == frame2.entrokoi);
    assert(restored_frame.xenon == frame2.xenon);
    assert(restored_frame.guff.world == frame2.guff.world);
    assert(restored_frame.guff.revision == frame2.guff.revision);
    assert(restored_frame.guff.world_time == frame2.guff.world_time);

    const auto restored_encoded = encode_snapshot(restored_frame.guff.snapshot);
    assert(restored_encoded);
    assert(restored_encoded.value() == encoded.value());

    assert(!restored.topology().directly_traversable(
        slice.package.zones.superbloom_east_edge,
        slice.package.zones.tecolote));
    assert(!restored.topology().directly_traversable(
        slice.package.zones.belmont_park,
        slice.package.zones.belmont_broken_coaster_edge));
    assert(!restored.topology().directly_traversable(
        slice.package.zones.mission_bay_waters,
        slice.package.zones.fiesta_island));

    return 0;
}
