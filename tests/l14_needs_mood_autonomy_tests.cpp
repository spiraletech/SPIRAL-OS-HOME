#include "home/versioned_world.hpp"

#include <cassert>
#include <variant>

int main() {
    using namespace home;

    // Mood bands are derived from numeric valence, never independently authored truth.
    assert(derive_mood_band(-8'000) == MoodBand::Distressed);
    assert(derive_mood_band(-2'000) == MoodBand::Low);
    assert(derive_mood_band(0) == MoodBand::Neutral);
    assert(derive_mood_band(2'000) == MoodBand::Positive);
    assert(derive_mood_band(8'000) == MoodBand::Elevated);

    EntityRegistry entities{WorldId{1}};
    const auto player = entities.create(EntityCreateInfo{EntityKind::Avatar, "operator", "Operator"});
    assert(player.ok());
    const auto unbound = entities.create(EntityCreateInfo{EntityKind::Avatar, "unbound", "Unbound"});
    assert(unbound.ok());

    TopologyRegistry topology{WorldId{1}};
    const auto home_zone = topology.create_zone(ZoneCreateInfo{ZoneKind::Interior, "home", "Home"});
    assert(home_zone.ok());

    PlayerLifeLedger life;
    PlayerLifeState life_state{};
    life_state.entity = player.value();
    life_state.home_zone = home_zone.value();
    life_state.born_world_minute = 0;
    life_state.updated_world_minute = 0;
    life_state.sequence = 1;
    assert(life.set(entities, topology, life_state).ok());

    PlayerDynamicsLedger dynamics;
    PlayerDynamicsState state{};
    state.entity = player.value();
    state.needs.set(NeedKind::Energy, 8'000);
    state.needs.set(NeedKind::Hunger, 6'500);
    state.mood = MoodState{2'000, 3'500, MoodBand::Positive};
    state.autonomy = AutonomyPolicy{AutonomyMode::Bounded, 4, true, true};
    state.active_drive = "eat";
    state.updated_world_minute = 0;
    state.sequence = 1;

    assert(dynamics.set(entities, life, state).ok());
    const auto bounded_decision = resolve_autonomy_decision(state);
    assert(bounded_decision.ok());
    assert(bounded_decision.value().intent == AutonomyIntent::Eat);
    assert(bounded_decision.value().governing_need == NeedKind::Hunger);
    assert(bounded_decision.value().initiative_allowed);
    assert(bounded_decision.value().drive == "eat");

    // Advisory mode resolves the same semantic intent but cannot authorize initiative.
    PlayerDynamicsState advisory = state;
    advisory.autonomy.mode = AutonomyMode::Advisory;
    const auto advisory_decision = resolve_autonomy_decision(advisory);
    assert(advisory_decision.ok());
    assert(advisory_decision.value().intent == AutonomyIntent::Eat);
    assert(!advisory_decision.value().initiative_allowed);

    // Lowest-need ties are deterministic by NeedKind order: Energy wins over Hunger.
    PlayerDynamicsState tie = state;
    tie.needs.set(NeedKind::Energy, 5'000);
    tie.needs.set(NeedKind::Hunger, 5'000);
    tie.active_drive = "rest";
    const auto tie_decision = resolve_autonomy_decision(tie);
    assert(tie_decision.ok());
    assert(tie_decision.value().governing_need == NeedKind::Energy);
    assert(tie_decision.value().intent == AutonomyIntent::Rest);

    PlayerDynamicsState contradictory_mood = state;
    contradictory_mood.mood.band = MoodBand::Distressed;
    assert(!validate_player_dynamics_shape(contradictory_mood).ok());

    PlayerDynamicsState contradictory_drive = state;
    contradictory_drive.active_drive = "rest";
    assert(!validate_player_dynamics_shape(contradictory_drive).ok());

    PlayerDynamicsState invalid_need = state;
    invalid_need.sequence = 2;
    invalid_need.needs.set(NeedKind::Safety, 10'001);
    assert(!dynamics.set(entities, life, invalid_need).ok());

    PlayerDynamicsState invalid_autonomy = state;
    invalid_autonomy.sequence = 2;
    invalid_autonomy.autonomy = AutonomyPolicy{AutonomyMode::Disabled, 1, false, false};
    invalid_autonomy.active_drive.clear();
    assert(!dynamics.set(entities, life, invalid_autonomy).ok());

    PlayerDynamicsState missing_life = state;
    missing_life.entity = unbound.value();
    assert(!dynamics.set(entities, life, missing_life).ok());

    PlayerDynamicsState stale = state;
    stale.sequence = 1;
    stale.needs.set(NeedKind::Hunger, 1'000);
    stale.active_drive = "eat";
    const auto stale_result = dynamics.set(entities, life, stale);
    assert(!stale_result.ok());
    assert(stale_result.error().code == ErrorCode::RevisionConflict);

    PlayerDynamicsState update = state;
    update.sequence = 2;
    update.needs.set(NeedKind::Energy, 5'000);
    update.needs.set(NeedKind::Hunger, 8'000);
    update.mood = MoodState{-2'000, 2'000, MoodBand::Low};
    update.active_drive = "rest";
    assert(dynamics.set(entities, life, update).ok());
    assert(dynamics.find(player.value())->needs.get(NeedKind::Energy) == 5'000);
    assert(dynamics.find(player.value())->mood.band == MoodBand::Low);

    // Canonical HOME integration: dynamics mutations are revisioned world truth.
    VersionedWorld world{WorldId{14}};
    const auto avatar = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "player", "Player"});
    assert(avatar.ok());
    const auto room = world.create_zone(ZoneCreateInfo{ZoneKind::Interior, "room", "Room"});
    assert(room.ok());

    PlayerLifeState canonical_life{};
    canonical_life.entity = avatar.value();
    canonical_life.home_zone = room.value();
    canonical_life.born_world_minute = 0;
    canonical_life.updated_world_minute = 0;
    canonical_life.sequence = 1;
    assert(world.set_player_life_state(canonical_life).ok());
    assert(world.revision() == WorldRevision{3});

    PlayerDynamicsState canonical{};
    canonical.entity = avatar.value();
    canonical.needs.set(NeedKind::Energy, 9'000);
    canonical.needs.set(NeedKind::Hunger, 6'000);
    canonical.mood = MoodState{0, 2'000, MoodBand::Neutral};
    canonical.autonomy = AutonomyPolicy{AutonomyMode::Bounded, 3, true, true};
    canonical.active_drive = "eat";
    canonical.updated_world_minute = 0;
    canonical.sequence = 1;
    assert(world.set_player_dynamics_state(canonical).ok());
    assert(world.revision() == WorldRevision{4});
    assert(world.player_dynamics().find(avatar.value()) != nullptr);
    assert(world.history().back().changes().size() == 1);
    assert(world.history().back().changes().front().kind == WorldChangeKind::PlayerDynamicsStateChanged);
    const auto& dynamics_change = std::get<PlayerDynamicsStateChanged>(world.history().back().changes().front().payload);
    assert(!dynamics_change.before.has_value());
    assert(dynamics_change.after.has_value());
    assert(*dynamics_change.after == canonical);

    const auto world_decision = world.resolve_player_autonomy(avatar.value());
    assert(world_decision.ok());
    assert(world_decision.value().intent == AutonomyIntent::Eat);

    const WorldRevision before_invalid = world.revision();
    PlayerDynamicsState future = canonical;
    future.sequence = 2;
    future.updated_world_minute = 1;
    const auto future_result = world.set_player_dynamics_state(future);
    assert(!future_result.ok());
    assert(world.revision() == before_invalid);

    PlayerDynamicsState canonical_update = canonical;
    canonical_update.sequence = 2;
    canonical_update.needs.set(NeedKind::Energy, 5'000);
    canonical_update.needs.set(NeedKind::Hunger, 8'000);
    canonical_update.mood = MoodState{-2'000, 4'000, MoodBand::Low};
    canonical_update.active_drive = "rest";
    assert(world.set_player_dynamics_state(canonical_update).ok());
    assert(world.revision() == WorldRevision{5});

    // Snapshot v8 preserves exact dynamics and deterministic autonomy inputs.
    const auto encoded = encode_snapshot(world.snapshot());
    assert(encoded.ok());
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().player_dynamics.size() == 1);
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    const VersionedWorld restored = restored_result.value();
    assert(restored.revision() == world.revision());
    assert(restored.player_dynamics().snapshot() == world.player_dynamics().snapshot());
    const auto reencoded = encode_snapshot(restored.snapshot());
    assert(reencoded.ok());
    assert(reencoded.value() == encoded.value());

    // v7 remains readable and naturally restores with no L14 dynamics state.
    const auto legacy = decode_snapshot(
        "HOME_SNAPSHOT 7\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "CLIMATES 0\n"
        "WEATHER 0\n"
        "AFFECT 0 0 0 1000\n"
        "ANCHOR 0 0 0 \"\"\n"
        "LIFE 0\n"
        "ENTITIES 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(legacy.ok());
    assert(legacy.value().player_dynamics.empty());

    // v8 refuses dynamics that have no matching L13 life state.
    const auto orphan = decode_snapshot(
        "HOME_SNAPSHOT 8\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "CLIMATES 0\n"
        "WEATHER 0\n"
        "AFFECT 0 0 0 1000\n"
        "ANCHOR 0 0 0 \"\"\n"
        "LIFE 0\n"
        "DYNAMICS 1\n"
        "D 1 10000 6000 10000 10000 10000 10000 0 2000 2 2 3 1 1 0 1 \"eat\"\n"
        "ENTITIES 1\n"
        "E 1 1 1 \"avatar\" \"Avatar\" 0 0 0 0 0 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(!orphan.ok());

    // Direct avatar deletion removes both L14 dynamics and L13 life atomically.
    const auto removed = world.remove_entity(avatar.value());
    assert(removed.ok());
    assert(world.player_dynamics().find(avatar.value()) == nullptr);
    assert(world.player_life().find(avatar.value()) == nullptr);
    bool saw_dynamics_removal = false;
    bool saw_life_removal = false;
    for (const auto& change : world.history().back().changes()) {
        if (change.kind == WorldChangeKind::PlayerDynamicsStateChanged) saw_dynamics_removal = true;
        if (change.kind == WorldChangeKind::PlayerLifeStateChanged) saw_life_removal = true;
    }
    assert(saw_dynamics_removal && saw_life_removal);

    // Transactional deletion must preserve the same cleanup law.
    VersionedWorld tx_world{WorldId{15}};
    const auto tx_avatar = tx_world.create_entity(EntityCreateInfo{EntityKind::Avatar, "tx-player", "Tx Player"});
    assert(tx_avatar.ok());
    PlayerLifeState tx_life{};
    tx_life.entity = tx_avatar.value();
    tx_life.sequence = 1;
    assert(tx_world.set_player_life_state(tx_life).ok());
    PlayerDynamicsState tx_dynamics{};
    tx_dynamics.entity = tx_avatar.value();
    tx_dynamics.mood = MoodState{0, 0, MoodBand::Neutral};
    tx_dynamics.autonomy = AutonomyPolicy{AutonomyMode::Disabled, 0, false, false};
    tx_dynamics.sequence = 1;
    assert(tx_world.set_player_dynamics_state(tx_dynamics).ok());

    WorldTransaction tx{};
    tx.id = WorldTransactionId{77};
    tx.expected_revision = tx_world.revision();
    tx.authority = "l14-test";
    tx.operations.push_back(TxRemoveEntity{tx_avatar.value()});
    const auto receipt = tx_world.execute(tx);
    assert(receipt.ok());
    assert(tx_world.player_dynamics().find(tx_avatar.value()) == nullptr);
    assert(tx_world.player_life().find(tx_avatar.value()) == nullptr);

    return 0;
}
