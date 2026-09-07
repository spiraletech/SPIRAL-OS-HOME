#include "home/player_life.hpp"

#include <cassert>

int main() {
    using namespace home;

    EntityRegistry entities{WorldId{1}};
    const auto player = entities.create(EntityCreateInfo{EntityKind::Avatar, "operator", "Operator"});
    assert(player.ok());

    TopologyRegistry topology{WorldId{1}};
    const auto home_zone = topology.create_zone(ZoneCreateInfo{ZoneKind::Interior, "home", "Home"});
    assert(home_zone.ok());

    PlayerLifeLedger ledger;
    PlayerLifeState state{};
    state.entity = player.value();
    state.stage = LifeStage::Adult;
    state.presence = LifePresence::Present;
    state.home_zone = home_zone.value();
    state.born_world_minute = 10;
    state.updated_world_minute = 20;
    state.sequence = 1;

    const auto created = ledger.set(entities, topology, state);
    assert(created.ok());
    assert(ledger.find(player.value()) != nullptr);
    assert(ledger.find(player.value())->home_zone == home_zone.value());

    PlayerLifeState stale = state;
    stale.presence = LifePresence::Away;
    const auto stale_result = ledger.set(entities, topology, stale);
    assert(!stale_result.ok());
    assert(stale_result.error().code == ErrorCode::RevisionConflict);
    assert(ledger.find(player.value())->presence == LifePresence::Present);

    PlayerLifeState update = state;
    update.presence = LifePresence::Away;
    update.updated_world_minute = 30;
    update.sequence = 2;
    const auto updated = ledger.set(entities, topology, update);
    assert(updated.ok());
    assert(ledger.find(player.value())->presence == LifePresence::Away);

    PlayerLifeState bad_zone = update;
    bad_zone.home_zone = ZoneId{999};
    bad_zone.sequence = 3;
    const auto rejected_zone = ledger.set(entities, topology, bad_zone);
    assert(!rejected_zone.ok());
    assert(rejected_zone.error().code == ErrorCode::NotFound);

    const auto snapshot = ledger.snapshot();
    assert(snapshot.size() == 1);
    assert(snapshot.front().entity == player.value());
    assert(snapshot.front().sequence == 2);
    return 0;
}
