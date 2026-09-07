#include "home/needs_mood_autonomy.hpp"

#include <cassert>

int main() {
    using namespace home;

    EntityRegistry entities{WorldId{1}};
    const auto player = entities.create(EntityCreateInfo{EntityKind::Avatar, "operator", "Operator"});
    assert(player.ok());

    TopologyRegistry topology{WorldId{1}};
    const auto home_zone = topology.create_zone(ZoneCreateInfo{ZoneKind::Interior, "home", "Home"});
    assert(home_zone.ok());

    PlayerLifeLedger life;
    PlayerLifeState life_state{};
    life_state.entity = player.value();
    life_state.home_zone = home_zone.value();
    life_state.born_world_minute = 1;
    life_state.updated_world_minute = 10;
    life_state.sequence = 1;
    assert(life.set(entities, topology, life_state).ok());

    PlayerDynamicsLedger dynamics;
    PlayerDynamicsState state{};
    state.entity = player.value();
    state.needs.set(NeedKind::Energy, 8'000);
    state.needs.set(NeedKind::Hunger, 6'500);
    state.mood = MoodState{2'000, 3'500, MoodBand::Positive};
    state.autonomy = AutonomyPolicy{AutonomyMode::Bounded, 4, true, true};
    state.active_drive = "rest";
    state.updated_world_minute = 20;
    state.sequence = 1;

    assert(dynamics.set(entities, life, state).ok());
    assert(dynamics.find(player.value()) != nullptr);
    assert(dynamics.find(player.value())->needs.get(NeedKind::Energy) == 8'000);
    assert(dynamics.find(player.value())->active_drive == "rest");

    PlayerDynamicsState stale = state;
    stale.sequence = 1;
    stale.needs.set(NeedKind::Energy, 1'000);
    const auto stale_result = dynamics.set(entities, life, stale);
    assert(!stale_result.ok());
    assert(stale_result.error().code == ErrorCode::RevisionConflict);
    assert(dynamics.find(player.value())->needs.get(NeedKind::Energy) == 8'000);

    PlayerDynamicsState invalid_need = state;
    invalid_need.sequence = 2;
    invalid_need.updated_world_minute = 21;
    invalid_need.needs.set(NeedKind::Safety, 10'001);
    const auto invalid_need_result = dynamics.set(entities, life, invalid_need);
    assert(!invalid_need_result.ok());
    assert(invalid_need_result.error().code == ErrorCode::ValidationFailed);

    PlayerDynamicsState invalid_autonomy = state;
    invalid_autonomy.sequence = 2;
    invalid_autonomy.updated_world_minute = 21;
    invalid_autonomy.autonomy = AutonomyPolicy{AutonomyMode::Disabled, 1, false, false};
    const auto invalid_autonomy_result = dynamics.set(entities, life, invalid_autonomy);
    assert(!invalid_autonomy_result.ok());
    assert(invalid_autonomy_result.error().code == ErrorCode::ValidationFailed);

    PlayerDynamicsState update = state;
    update.sequence = 2;
    update.updated_world_minute = 30;
    update.needs.set(NeedKind::Energy, 7'000);
    update.mood = MoodState{-1'500, 2'000, MoodBand::Low};
    update.active_drive = "sleep";
    assert(dynamics.set(entities, life, update).ok());
    assert(dynamics.find(player.value())->needs.get(NeedKind::Energy) == 7'000);
    assert(dynamics.find(player.value())->mood.band == MoodBand::Low);

    PlayerDynamicsState regressed = update;
    regressed.sequence = 3;
    regressed.updated_world_minute = 29;
    const auto regressed_result = dynamics.set(entities, life, regressed);
    assert(!regressed_result.ok());
    assert(regressed_result.error().code == ErrorCode::RevisionConflict);

    EntityRegistry other_entities{WorldId{2}};
    const auto outsider = other_entities.create(EntityCreateInfo{EntityKind::Avatar, "outsider", "Outsider"});
    assert(outsider.ok());
    PlayerDynamicsState missing_life = state;
    missing_life.entity = outsider.value();
    const auto missing_life_result = dynamics.set(other_entities, life, missing_life);
    assert(!missing_life_result.ok());
    assert(missing_life_result.error().code == ErrorCode::ValidationFailed);

    const auto snapshot = dynamics.snapshot();
    assert(snapshot.size() == 1);
    assert(snapshot.front().entity == player.value());
    assert(snapshot.front().sequence == 2);
    return 0;
}
