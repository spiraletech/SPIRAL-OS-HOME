#include "home/life_state.hpp"

#include <cassert>

int main() {
    using namespace home;
    LifeLedger ledger;
    PlayerLifeState player{}; player.player=PlayerId{1}; player.avatar=EntityId{7}; player.display_name="Agnathos";
    assert(ledger.create(player).ok());
    assert(ledger.find(PlayerId{1}) != nullptr);
    assert(ledger.find(PlayerId{1})->life_revision == 0);
    assert(ledger.set_home(PlayerId{1}, ZoneId{3}).ok());
    assert(ledger.set_current_zone(PlayerId{1}, ZoneId{4}).ok());
    assert(ledger.find(PlayerId{1})->home_zone == ZoneId{3});
    assert(ledger.find(PlayerId{1})->current_zone == ZoneId{4});
    assert(ledger.find(PlayerId{1})->life_revision == 2);
    assert(ledger.set_active(PlayerId{1}, false).ok());
    assert(!ledger.find(PlayerId{1})->active);
    const auto duplicate=ledger.create(player); assert(!duplicate.ok()); assert(duplicate.error().code==ErrorCode::AlreadyExists);
    PlayerLifeState same_avatar{}; same_avatar.player=PlayerId{2}; same_avatar.avatar=EntityId{7}; same_avatar.display_name="Other";
    assert(!ledger.create(same_avatar).ok());
    assert(ledger.snapshot().size()==1);
    return 0;
}
