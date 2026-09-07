#include "home/theorism.hpp"

#include <cassert>

int main() {
    using namespace home;

    TheorismLedger ledger;
    const PlayerId p1{1};
    const PlayerId p2{2};

    assert(ledger.set_subclass(p2, "observer", 3));
    assert(ledger.set_subclass(p1, "runner", 1));
    assert(ledger.find_subclass(p1));
    assert(ledger.find_subclass(p1)->key == "runner");
    assert(ledger.set_subclass_rank(p1, 2));
    assert(ledger.find_subclass(p1)->rank == 2);
    assert(!ledger.set_subclass_rank(p1, 0));
    assert(ledger.subclass_snapshot().front().player == p1);

    assert(ledger.set_theorism(p1, "spiral", 400));
    assert(ledger.find_theorism(p1)->conviction == 400);
    assert(ledger.set_theorism_conviction(p1, 1000));
    assert(ledger.find_theorism(p1)->theorism_revision == 2);
    assert(!ledger.set_theorism_conviction(p1, 1001));

    assert(ledger.set_aura(p1, "calm-static", 250, -100));
    assert(ledger.find_aura(p1)->signature == "calm-static");
    assert(ledger.set_aura_intensity(p1, 800));
    assert(ledger.set_aura_charge(p1, 500));
    assert(ledger.find_aura(p1)->intensity == 800);
    assert(ledger.find_aura(p1)->charge == 500);
    assert(ledger.find_aura(p1)->aura_revision == 3);
    assert(!ledger.set_aura(p2, "", 1, 0));
    assert(!ledger.set_aura(p2, "bad", 1001, 0));
    assert(!ledger.set_aura(p2, "bad", 10, 1001));

    assert(ledger.clear_subclass(p1));
    assert(!ledger.find_subclass(p1));
    assert(ledger.clear_theorism(p1));
    assert(!ledger.find_theorism(p1));
    assert(ledger.clear_aura(p1));
    assert(!ledger.find_aura(p1));

    return 0;
}
