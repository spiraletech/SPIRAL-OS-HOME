#include "home/home_v01_vertical_slice.hpp"

#include <utility>

namespace home {
namespace {

Result<HomeV01SynchronizedFrame> sync_failure(const char* message) {
    return Result<HomeV01SynchronizedFrame>::failure(ErrorCode::InternalError, message);
}

} // namespace

Result<HomeV01SynchronizedFrame> project_home_v01_frame(
    VersionedWorld& world,
    EntityId observer,
    const GuffHomeGrant& guff_grant) {
    HakuiAdapter hakui_adapter{world};
    InokuiAdapter inokui_adapter{world};
    EntrokoiAdapter entrokoi_adapter{world};
    XenonAdapter xenon_adapter{world};
    GuffHomeCartridge guff_cartridge{world, guff_grant};

    const HakuiFrame hakui = hakui_adapter.project();
    const auto inokui_result = inokui_adapter.snapshot();
    if (!inokui_result) {
        return Result<HomeV01SynchronizedFrame>::failure(
            inokui_result.error().code, inokui_result.error().message);
    }
    const auto entrokoi_result = entrokoi_adapter.snapshot_for(observer);
    if (!entrokoi_result) {
        return Result<HomeV01SynchronizedFrame>::failure(
            entrokoi_result.error().code, entrokoi_result.error().message);
    }
    const auto xenon_result = xenon_adapter.snapshot();
    if (!xenon_result) {
        return Result<HomeV01SynchronizedFrame>::failure(
            xenon_result.error().code, xenon_result.error().message);
    }
    const GuffHomeContext guff = guff_cartridge.context();

    const WorldId canonical_world = world.world();
    const WorldRevision canonical_revision = world.revision();
    const WorldTime canonical_time = world.clock().now();
    const auto& inokui = inokui_result.value();
    const auto& entrokoi = entrokoi_result.value();
    const auto& xenon = xenon_result.value();

    if (hakui.world != canonical_world || inokui.world != canonical_world ||
        entrokoi.world != canonical_world || xenon.world != canonical_world ||
        guff.world != canonical_world) {
        return sync_failure("HOME v0.1 integration views disagree on WorldId");
    }
    if (hakui.revision != canonical_revision || inokui.revision != canonical_revision ||
        entrokoi.revision != canonical_revision || xenon.revision != canonical_revision ||
        guff.revision != canonical_revision || guff.snapshot.revision != canonical_revision) {
        return sync_failure("HOME v0.1 integration views disagree on WorldRevision");
    }
    if (hakui.world_time != canonical_time || inokui.world_time != canonical_time ||
        entrokoi.world_time != canonical_time || xenon.world_time != canonical_time ||
        guff.world_time != canonical_time) {
        return sync_failure("HOME v0.1 integration views disagree on WorldTime");
    }
    if (entrokoi.observer.entity != observer) {
        return sync_failure("HOME v0.1 ENTROKOI observer does not match the vertical-slice player");
    }

    HomeV01SynchronizedFrame frame{};
    frame.world = canonical_world;
    frame.revision = canonical_revision;
    frame.world_time = canonical_time;
    frame.hakui = hakui;
    frame.inokui = inokui_result.value();
    frame.entrokoi = entrokoi_result.value();
    frame.xenon = xenon_result.value();
    frame.guff = guff;
    return Result<HomeV01SynchronizedFrame>::success(std::move(frame));
}

Result<HomeV01SynchronizedFrame> HomeV01VerticalSlice::project() {
    return project_home_v01_frame(package.world, player, guff_grant);
}

Result<HakuiApplyReceipt> HomeV01VerticalSlice::apply_hakui(const HakuiConsequenceBatch& batch) {
    HakuiAdapter adapter{package.world};
    return adapter.apply(batch);
}

Result<GuffHomeDecision> HomeV01VerticalSlice::submit_guff(const GuffHomeProposal& proposal) {
    GuffHomeCartridge cartridge{package.world, guff_grant};
    return cartridge.submit(proposal);
}

Result<HomeV01VerticalSlice> make_home_v01_vertical_slice(WorldId world_id) {
    auto package_result = make_mission_bay_world(world_id);
    if (!package_result) {
        return Result<HomeV01VerticalSlice>::failure(
            package_result.error().code, package_result.error().message);
    }
    MissionBayWorldPackage package = std::move(package_result).value();

    const auto player_result = package.world.create_entity(EntityCreateInfo{
        .kind = EntityKind::Avatar,
        .archetype = "agnathos",
        .display_name = "Agnathos",
        .transform = {},
        .persistent = true,
    });
    if (!player_result) {
        return Result<HomeV01VerticalSlice>::failure(
            player_result.error().code, player_result.error().message);
    }
    const EntityId player = player_result.value();

    const auto placed = package.world.place_entity(player, package.zones.crown_point);
    if (!placed) {
        return Result<HomeV01VerticalSlice>::failure(placed.error().code, placed.error().message);
    }

    PlayerLifeState life{};
    life.entity = player;
    life.stage = LifeStage::Adult;
    life.presence = LifePresence::Present;
    life.home_zone = package.zones.crown_point;
    life.born_world_minute = 0;
    life.updated_world_minute = 0;
    life.sequence = 1;
    const auto life_result = package.world.set_player_life_state(life);
    if (!life_result) {
        return Result<HomeV01VerticalSlice>::failure(life_result.error().code, life_result.error().message);
    }

    PlayerDynamicsState dynamics{};
    dynamics.entity = player;
    dynamics.mood = MoodState{2'500, 4'500, MoodBand::Positive};
    dynamics.autonomy = AutonomyPolicy{AutonomyMode::Disabled, 0, false, false};
    dynamics.updated_world_minute = 0;
    dynamics.sequence = 1;
    const auto dynamics_result = package.world.set_player_dynamics_state(dynamics);
    if (!dynamics_result) {
        return Result<HomeV01VerticalSlice>::failure(
            dynamics_result.error().code, dynamics_result.error().message);
    }

    SubclassAffinityState skater{};
    skater.player = player;
    skater.key = "street-skater";
    skater.evidence_points = 700;
    skater.affinity_permille = subclass_affinity_for_evidence(skater.evidence_points);
    skater.discovered_world_minute = 0;
    skater.updated_world_minute = 0;
    skater.sequence = 1;
    const auto subclass_result = package.world.set_subclass_affinity(skater);
    if (!subclass_result) {
        return Result<HomeV01VerticalSlice>::failure(
            subclass_result.error().code, subclass_result.error().message);
    }

    ItemCreateInfo skateboard_info{};
    skateboard_info.archetype_key = "equipment.skateboard";
    skateboard_info.display_name = "Skateboard";
    skateboard_info.kind = ItemKind::Equipment;
    skateboard_info.quantity = 1;
    skateboard_info.max_stack = 1;
    skateboard_info.durability = kItemDurabilityMaximum;
    skateboard_info.owner = player;
    skateboard_info.updated_world_minute = 0;
    const auto skateboard_result = package.world.create_item(skateboard_info);
    if (!skateboard_result) {
        return Result<HomeV01VerticalSlice>::failure(
            skateboard_result.error().code, skateboard_result.error().message);
    }

    GuffHomeGrant grant{};
    grant.principal = "home-v01-planner";
    grant.capabilities = {
        GuffHomeCapability::UpdateTransform,
        GuffHomeCapability::PlaceEntity,
    };
    grant.max_operations_per_proposal = 4;

    HomeV01VerticalSlice slice{
        .package = std::move(package),
        .player = player,
        .skateboard = skateboard_result.value(),
        .guff_grant = std::move(grant),
    };

    const auto synchronized = slice.project();
    if (!synchronized) {
        return Result<HomeV01VerticalSlice>::failure(
            synchronized.error().code, synchronized.error().message);
    }

    return Result<HomeV01VerticalSlice>::success(std::move(slice));
}

} // namespace home
