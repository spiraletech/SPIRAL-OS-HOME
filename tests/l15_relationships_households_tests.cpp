#include "home/versioned_world.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <variant>

namespace {

home::PlayerLifeState life_for(home::EntityId entity, home::ZoneId home_zone, std::uint64_t minute) {
    home::PlayerLifeState state{};
    state.entity = entity;
    state.stage = home::LifeStage::Adult;
    state.presence = home::LifePresence::Present;
    state.home_zone = home_zone;
    state.born_world_minute = 0;
    state.updated_world_minute = minute;
    state.sequence = 1;
    return state;
}

} // namespace

int main() {
    using namespace home;

    VersionedWorld world{WorldId{15}};
    const auto advanced = world.advance_time(300000); // 10 HOME minutes.
    assert(advanced.ok());
    const auto home_zone = world.create_zone(ZoneCreateInfo{ZoneKind::Interior, "house", "House"});
    assert(home_zone.ok());

    const auto alice = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "alice", "Alice"});
    const auto bob = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "bob", "Bob"});
    const auto cara = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "cara", "Cara"});
    assert(alice.ok() && bob.ok() && cara.ok());
    assert(world.set_player_life_state(life_for(alice.value(), home_zone.value(), 10)).ok());
    assert(world.set_player_life_state(life_for(bob.value(), home_zone.value(), 10)).ok());
    assert(world.set_player_life_state(life_for(cara.value(), home_zone.value(), 10)).ok());

    RelationshipState alice_to_bob{};
    alice_to_bob.from = alice.value();
    alice_to_bob.to = bob.value();
    alice_to_bob.kind = RelationshipKind::Friend;
    alice_to_bob.affinity = 7000;
    alice_to_bob.trust = 8000;
    alice_to_bob.updated_world_minute = 10;
    alice_to_bob.sequence = 1;
    assert(world.set_relationship_state(alice_to_bob).ok());

    RelationshipState bob_to_alice = alice_to_bob;
    bob_to_alice.from = bob.value();
    bob_to_alice.to = alice.value();
    bob_to_alice.kind = RelationshipKind::Rival;
    bob_to_alice.affinity = -4000;
    bob_to_alice.trust = 1500;
    assert(world.set_relationship_state(bob_to_alice).ok());
    assert(world.relationships().find_relationship(alice.value(), bob.value())->kind == RelationshipKind::Friend);
    assert(world.relationships().find_relationship(bob.value(), alice.value())->kind == RelationshipKind::Rival);

    const WorldRevision before_stale = world.revision();
    RelationshipState stale = alice_to_bob;
    stale.affinity = 9000;
    assert(!world.set_relationship_state(stale).ok());
    assert(world.revision() == before_stale);
    assert(world.relationships().find_relationship(alice.value(), bob.value())->affinity == 7000);

    RelationshipState future = alice_to_bob;
    future.to = cara.value();
    future.updated_world_minute = 11;
    assert(!world.set_relationship_state(future).ok());
    RelationshipState self = alice_to_bob;
    self.to = alice.value();
    assert(!world.set_relationship_state(self).ok());

    HouseholdCreateInfo create{};
    create.name = "Bay House";
    create.members = {bob.value(), alice.value()};
    create.home_zone = home_zone.value();
    create.updated_world_minute = 10;
    const auto household_id = world.create_household(create);
    assert(household_id.ok());
    const auto* household = world.relationships().find_household(household_id.value());
    assert(household != nullptr);
    assert(household->members.size() == 2);
    assert(household->members[0] == alice.value());
    assert(household->members[1] == bob.value());

    HouseholdCreateInfo conflict = create;
    conflict.name = "Conflict House";
    conflict.members = {bob.value(), cara.value()};
    const WorldRevision before_conflict = world.revision();
    assert(!world.create_household(conflict).ok());
    assert(world.revision() == before_conflict);

    HouseholdState expanded = *household;
    expanded.members.push_back(cara.value());
    expanded.updated_world_minute = 10;
    expanded.sequence = 2;
    assert(world.set_household_state(expanded).ok());
    assert(world.relationships().household_for(cara.value())->id == household_id.value());

    RelationshipState alice_to_cara{};
    alice_to_cara.from = alice.value();
    alice_to_cara.to = cara.value();
    alice_to_cara.kind = RelationshipKind::Family;
    alice_to_cara.affinity = 6000;
    alice_to_cara.trust = 9000;
    alice_to_cara.updated_world_minute = 10;
    alice_to_cara.sequence = 1;
    assert(world.set_relationship_state(alice_to_cara).ok());

    const auto removed_bob = world.remove_entity(bob.value());
    assert(removed_bob.ok());
    assert(world.relationships().find_relationship(alice.value(), bob.value()) == nullptr);
    assert(world.relationships().find_relationship(bob.value(), alice.value()) == nullptr);
    const auto* after_bob = world.relationships().find_household(household_id.value());
    assert(after_bob != nullptr);
    assert(after_bob->members.size() == 2);
    assert(std::find(after_bob->members.begin(), after_bob->members.end(), bob.value()) == after_bob->members.end());
    assert(world.history().back().changes().end() != std::find_if(
        world.history().back().changes().begin(), world.history().back().changes().end(),
        [](const WorldChange& change) { return change.kind == WorldChangeKind::HouseholdStateChanged; }));

    const WorldSnapshot saved = world.snapshot();
    assert(saved.relationships.size() == 1);
    assert(saved.households.size() == 1);
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    assert(encoded.value().find("HOME_SNAPSHOT " + std::to_string(kSnapshotFormatVersion)) == 0);
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    VersionedWorld restored = restored_result.value();
    assert(restored.relationships().find_relationship(alice.value(), cara.value()) != nullptr);
    assert(restored.relationships().find_household(household_id.value()) != nullptr);
    assert(restored.revision() == world.revision());
    assert(restored.history().empty());

    HouseholdCreateInfo post_restore{};
    post_restore.name = "New House";
    post_restore.members = {alice.value()};
    post_restore.home_zone = home_zone.value();
    post_restore.updated_world_minute = 10;
    assert(restored.dissolve_household(household_id.value()).ok());
    const auto next_household = restored.create_household(post_restore);
    assert(next_household.ok());
    assert(next_household.value().value() > household_id.value().value());

    WorldTransaction tx{};
    tx.id = WorldTransactionId{1515};
    tx.expected_revision = world.revision();
    tx.authority = "home.l15.test";
    tx.operations.push_back(TxRemoveEntity{cara.value()});
    const auto receipt = world.execute(tx);
    assert(receipt.ok());
    assert(world.relationships().find_relationship(alice.value(), cara.value()) == nullptr);
    const auto* after_cara = world.relationships().find_household(household_id.value());
    assert(after_cara != nullptr);
    assert(after_cara->members.size() == 1);
    assert(after_cara->members.front() == alice.value());

    WorldSnapshot orphan = world.snapshot();
    RelationshipState orphan_relationship{};
    orphan_relationship.from = alice.value();
    orphan_relationship.to = EntityId{999999};
    orphan_relationship.kind = RelationshipKind::Friend;
    orphan_relationship.affinity = 0;
    orphan_relationship.trust = 1000;
    orphan_relationship.updated_world_minute = 10;
    orphan_relationship.sequence = 1;
    orphan.relationships.push_back(orphan_relationship);
    assert(!encode_snapshot(orphan).ok());

    const std::string legacy_v8 =
        "HOME_SNAPSHOT 8\n"
        "WORLD 99 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "CLIMATES 0\n"
        "WEATHER 0\n"
        "AFFECT 0 0 0 1000\n"
        "ANCHOR 0 0 0 \"\"\n"
        "LIFE 0\n"
        "DYNAMICS 0\n"
        "ENTITIES 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n";
    const auto legacy = decode_snapshot(legacy_v8);
    assert(legacy.ok());
    assert(legacy.value().relationships.empty());
    assert(legacy.value().households.empty());

    return 0;
}
