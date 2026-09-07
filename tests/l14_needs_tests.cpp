#include "home/needs.hpp"

#include <cassert>

int main() {
    using namespace home;

    const PlayerId player{7};
    NeedsLedger ledger;
    auto state = default_needs_state(player);
    assert(state.needs[static_cast<std::size_t>(NeedKind::Hunger)].value == kNeedMax);
    assert(ledger.create(state));
    assert(!ledger.create(state));

    assert(ledger.set_need(player, NeedKind::Hunger, 800));
    assert(ledger.set_decay(player, NeedKind::Hunger, 25));
    assert(ledger.apply_decay(player, 4));
    const auto* after_decay = ledger.find(player);
    assert(after_decay != nullptr);
    assert(after_decay->needs[static_cast<std::size_t>(NeedKind::Hunger)].value == 700);

    assert(ledger.set_mood(player, MoodState{250, 400}));
    assert(ledger.set_autonomy(player, AutonomyState{AutonomyMode::Autonomous, 300}));
    const auto* final = ledger.find(player);
    assert(final != nullptr);
    assert(final->mood.valence == 250);
    assert(final->mood.arousal == 400);
    assert(final->autonomy.mode == AutonomyMode::Autonomous);
    assert(final->autonomy.initiative_threshold == 300);
    assert(final->needs_revision == 5);

    assert(!ledger.set_need(player, NeedKind::Hunger, 1001));
    assert(!ledger.set_mood(player, MoodState{1001, 0}));
    assert(!ledger.set_autonomy(player, AutonomyState{AutonomyMode::Assisted, 1001}));
    assert(!ledger.apply_decay(player, 0));

    auto snapshot = ledger.snapshot();
    assert(snapshot.size() == 1);
    assert(snapshot.front().player == player);
    return 0;
}
