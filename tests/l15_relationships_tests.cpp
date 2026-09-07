#include "home/relationships.hpp"

#include <cassert>

using namespace home;

int main() {
    RelationshipsLedger ledger;
    const PlayerId a{11};
    const PlayerId b{22};
    const PlayerId c{33};
    const PlayerId d{44};

    RelationshipState friendship{};
    friendship.first = b;
    friendship.second = a;
    friendship.kind = RelationshipKind::Friend;
    friendship.affinity = 650;
    assert(ledger.create_relationship(friendship));

    const auto* stored = ledger.find_relationship(a, b);
    assert(stored);
    assert(stored->first == a);
    assert(stored->second == b);
    assert(stored->kind == RelationshipKind::Friend);
    assert(stored->affinity == 650);
    assert(stored->relationship_revision == 0);
    assert(ledger.find_relationship(b, a) == stored);

    assert(!ledger.create_relationship(friendship));
    assert(!ledger.create_relationship(RelationshipState{a, a, RelationshipKind::Family, 0, 0}));
    assert(ledger.set_affinity(b, a, 900));
    assert(ledger.find_relationship(a, b)->affinity == 900);
    assert(ledger.find_relationship(a, b)->relationship_revision == 1);
    assert(!ledger.set_affinity(a, b, 1200));
    assert(ledger.set_relationship_kind(a, b, RelationshipKind::Family));
    assert(ledger.find_relationship(a, b)->relationship_revision == 2);

    RelationshipState rivalry{d, c, RelationshipKind::Rival, -700, 0};
    assert(ledger.create_relationship(rivalry));
    const auto relationships = ledger.relationship_snapshot();
    assert(relationships.size() == 2);
    assert(relationships[0].first == a);
    assert(relationships[1].first == c);

    auto household_result = ledger.create_household("Mission Bay House", {c, a});
    assert(household_result);
    const HouseholdId home_id = household_result.value();
    const auto* household = ledger.find_household(home_id);
    assert(household);
    assert(household->members.size() == 2);
    assert(household->members[0] == a);
    assert(household->members[1] == c);
    assert(ledger.household_for(a)->id == home_id);

    assert(ledger.add_member(home_id, b));
    household = ledger.find_household(home_id);
    assert(household->members.size() == 3);
    assert(household->household_revision == 1);
    assert(!ledger.create_household("Other House", {b}));
    assert(!ledger.add_member(home_id, b));

    assert(ledger.rename_household(home_id, "Crown Point House"));
    household = ledger.find_household(home_id);
    assert(household->name == "Crown Point House");
    assert(household->household_revision == 2);

    assert(ledger.remove_member(home_id, c));
    assert(!ledger.household_for(c));
    assert(ledger.find_household(home_id)->household_revision == 3);

    auto second = ledger.create_household("Solo", {d});
    assert(second);
    const auto households = ledger.household_snapshot();
    assert(households.size() == 2);
    assert(households[0].id.value() < households[1].id.value());

    assert(ledger.remove_relationship(a, b));
    assert(!ledger.find_relationship(a, b));
    assert(ledger.dissolve_household(home_id));
    assert(!ledger.find_household(home_id));
    assert(!ledger.household_for(a));
    assert(!ledger.household_for(b));

    return 0;
}
