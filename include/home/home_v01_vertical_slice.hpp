#pragma once

#include "home/entrokoi_adapter.hpp"
#include "home/guff_home_cartridge.hpp"
#include "home/hakui_adapter.hpp"
#include "home/inokui_adapter.hpp"
#include "home/mission_bay_world.hpp"
#include "home/xenon_adapter.hpp"

namespace home {

struct HomeV01SynchronizedFrame final {
    WorldId world{};
    WorldRevision revision{};
    WorldTime world_time{};
    HakuiFrame hakui{};
    InokuiWorldView inokui{};
    EntrokoiWorldView entrokoi{};
    XenonWorldView xenon{};
    GuffHomeContext guff{};
};

struct HomeV01VerticalSlice final {
    MissionBayWorldPackage package;
    EntityId player{};
    ItemId skateboard{};
    GuffHomeGrant guff_grant{};

    [[nodiscard]] VersionedWorld& world() noexcept { return package.world; }
    [[nodiscard]] const VersionedWorld& world() const noexcept { return package.world; }

    [[nodiscard]] Result<HomeV01SynchronizedFrame> project();
    [[nodiscard]] Result<HakuiApplyReceipt> apply_hakui(const HakuiConsequenceBatch& batch);
    [[nodiscard]] Result<GuffHomeDecision> submit_guff(const GuffHomeProposal& proposal);
};

// Project every HOME integration boundary from one canonical revision. The helper is
// also used after snapshot restore to prove restored worlds produce the same engine
// inputs without re-authoring truth in downstream systems.
[[nodiscard]] Result<HomeV01SynchronizedFrame> project_home_v01_frame(
    VersionedWorld& world,
    EntityId observer,
    const GuffHomeGrant& guff_grant);

// Boots the deterministic Mission Bay package with one canonical living player,
// semantic aura state, and a HOME-owned skateboard inventory item.
[[nodiscard]] Result<HomeV01VerticalSlice> make_home_v01_vertical_slice(WorldId world_id);

} // namespace home
