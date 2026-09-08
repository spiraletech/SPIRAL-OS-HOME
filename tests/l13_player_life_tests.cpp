#include "home/versioned_world.hpp"

#include <cassert>
#include <variant>

int main() {
    using namespace home;

    // Standalone ledger contract: only avatars may own life state, chronology is monotonic.
    EntityRegistry entities{WorldId{1}};
    const auto player = entities.create(EntityCreateInfo{EntityKind::Avatar, "operator", "Operator"});
    const auto item = entities.create(EntityCreateInfo{EntityKind::Item, "board", "Board"});
    assert(player.ok() && item.ok());

    TopologyRegistry topology{WorldId{1}};
    const auto home_zone = topology.create_zone(ZoneCreateInfo{ZoneKind::Interior, "home", "Home"});
    assert(home_zone.ok());

    PlayerLifeLedger ledger;
    PlayerLifeState state{};
    state.entity = player.value();
    state.stage = LifeStage::Adult;
    state.presence = LifePresence::Present;
    state.home_zone = home_zone.value();
    state.born_world_minute = 0;
    state.updated_world_minute = 0;
    state.sequence = 1;
    assert(ledger.set(entities, topology, state).ok());

    PlayerLifeState item_life = state;
    item_life.entity = item.value();
    assert(!ledger.set(entities, topology, item_life).ok());

    PlayerLifeState stale = state;
    stale.presence = LifePresence::Away;
    assert(!ledger.set(entities, topology, stale).ok());

    PlayerLifeState birth_rewrite = state;
    birth_rewrite.born_world_minute = 1;
    birth_rewrite.updated_world_minute = 1;
    birth_rewrite.sequence = 2;
    assert(!ledger.set(entities, topology, birth_rewrite).ok());

    PlayerLifeState update = state;
    update.presence = LifePresence::Away;
    update.updated_world_minute = 1;
    update.sequence = 2;
    assert(ledger.set(entities, topology, update).ok());
    assert(ledger.find(player.value())->presence == LifePresence::Away);

    const auto removed_life = ledger.remove(player.value());
    assert(removed_life.ok());
    assert(ledger.find(player.value()) == nullptr);

    // HOME integration: player life is canonical, revisioned truth.
    VersionedWorld world{WorldId{13}};
    ZoneCreateInfo room_info{};
    room_info.kind = ZoneKind::Interior;
    room_info.key = "player-home";
    room_info.display_name = "Player Home";
    const auto room = world.create_zone(room_info);
    assert(room.ok());

    EntityCreateInfo avatar_info{};
    avatar_info.kind = EntityKind::Avatar;
    avatar_info.archetype = "spiral.avatar";
    avatar_info.display_name = "Resident";
    const auto avatar = world.create_entity(avatar_info);
    assert(avatar.ok());

    EntityCreateInfo world_item_info{};
    world_item_info.kind = EntityKind::Item;
    world_item_info.archetype = "item.skateboard";
    world_item_info.display_name = "Skateboard";
    const auto world_item = world.create_entity(world_item_info);
    assert(world_item.ok());
    assert(world.revision() == WorldRevision{3});

    PlayerLifeState canonical{};
    canonical.entity = avatar.value();
    canonical.stage = LifeStage::Adult;
    canonical.presence = LifePresence::Present;
    canonical.home_zone = room.value();
    canonical.born_world_minute = 0;
    canonical.updated_world_minute = 0;
    canonical.sequence = 1;
    assert(world.set_player_life_state(canonical).ok());
    assert(world.revision() == WorldRevision{4});
    assert(world.player_life().find(avatar.value()) != nullptr);
    assert(world.history().back().changes().size() == 1);
    assert(world.history().back().changes().front().kind == WorldChangeKind::PlayerLifeStateChanged);
    const auto& created_change = std::get<PlayerLifeStateChanged>(world.history().back().changes().front().payload);
    assert(!created_change.before.has_value());
    assert(created_change.after == canonical);

    const WorldRevision before_invalid = world.revision();
    PlayerLifeState non_avatar = canonical;
    non_avatar.entity = world_item.value();
    assert(!world.set_player_life_state(non_avatar).ok());
    assert(world.revision() == before_invalid);

    PlayerLifeState future = canonical;
    future.sequence = 2;
    future.updated_world_minute = 1;
    assert(!world.set_player_life_state(future).ok());
    assert(world.revision() == before_invalid);

    // 30 real seconds == one HOME minute, so minute 1 becomes a valid update timestamp.
    const auto advanced = world.advance_time(30000);
    assert(advanced.ok());
    assert(advanced.value().milliseconds == 60000);
    assert(world.revision() == WorldRevision{5});

    PlayerLifeState canonical_update = canonical;
    canonical_update.presence = LifePresence::Away;
    canonical_update.updated_world_minute = 1;
    canonical_update.sequence = 2;
    assert(world.set_player_life_state(canonical_update).ok());
    assert(world.revision() == WorldRevision{6});
    const auto& updated_change = std::get<PlayerLifeStateChanged>(world.history().back().changes().front().payload);
    assert(updated_change.before == canonical);
    assert(updated_change.after == canonical_update);

    PlayerLifeState rewrite_birth = canonical_update;
    rewrite_birth.born_world_minute = 1;
    rewrite_birth.sequence = 3;
    assert(!world.set_player_life_state(rewrite_birth).ok());
    assert(world.revision() == WorldRevision{6});

    // Snapshot v7 preserves exact life state without minting a reload revision.
    const WorldSnapshot saved = world.snapshot();
    assert(saved.player_life.size() == 1);
    assert(saved.player_life.front() == canonical_update);
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().player_life.size() == 1);
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    const VersionedWorld restored = restored_result.value();
    assert(restored.revision() == world.revision());
    assert(restored.player_life().snapshot() == world.player_life().snapshot());
    const auto reencoded = encode_snapshot(restored.snapshot());
    assert(reencoded.ok());
    assert(reencoded.value() == encoded.value());

    WorldSnapshot bad_item_life = saved;
    bad_item_life.player_life.front().entity = world_item.value();
    assert(!encode_snapshot(bad_item_life).ok());

    WorldSnapshot bad_future_life = saved;
    bad_future_life.player_life.front().updated_world_minute = 2;
    bad_future_life.player_life.front().sequence = 3;
    assert(!encode_snapshot(bad_future_life).ok());

    // v6 saves remain readable and naturally restore with no L13 state.
    const auto legacy = decode_snapshot(
        "HOME_SNAPSHOT 6\n"
        "WORLD 2 0\n"
        "CLOCK 30000 0 0\n"
        "CALENDAR 2026 1 1\n"
        "EVENTS 0\n"
        "CLIMATES 0\n"
        "WEATHER 0\n"
        "AFFECT 0 0 0 1000\n"
        "ANCHOR 0 0 0 \"\"\n"
        "ENTITIES 0\n"
        "ZONES 0\n"
        "CONNECTIONS 0\n"
        "PLACEMENTS 0\n"
        "END\n");
    assert(legacy.ok());
    assert(legacy.value().player_life.empty());

    // Direct avatar deletion atomically removes its canonical life record.
    const WorldRevision before_delete = world.revision();
    const auto removed_avatar = world.remove_entity(avatar.value());
    assert(removed_avatar.ok());
    assert(world.revision() == WorldRevision{before_delete.value() + 1});
    assert(world.player_life().find(avatar.value()) == nullptr);
    bool saw_life_removal = false;
    bool saw_entity_removal = false;
    for (const auto& change : world.history().back().changes()) {
        if (change.kind == WorldChangeKind::PlayerLifeStateChanged) saw_life_removal = true;
        if (change.kind == WorldChangeKind::EntityRemoved) saw_entity_removal = true;
    }
    assert(saw_life_removal && saw_entity_removal);

    // Transactional avatar deletion obeys the same invariant in one revision.
    VersionedWorld tx_world{WorldId{14}};
    const auto tx_room = tx_world.create_zone(room_info);
    const auto tx_avatar = tx_world.create_entity(avatar_info);
    assert(tx_room.ok() && tx_avatar.ok());
    PlayerLifeState tx_life{};
    tx_life.entity = tx_avatar.value();
    tx_life.home_zone = tx_room.value();
    tx_life.sequence = 1;
    assert(tx_world.set_player_life_state(tx_life).ok());

    const WorldRevision before_tx = tx_world.revision();
    WorldTransaction tx{};
    tx.id = WorldTransactionId{13};
    tx.expected_revision = before_tx;
    tx.authority = "home.l13.test";
    tx.operations = {TxRemoveEntity{tx_avatar.value()}};
    const auto tx_removed = tx_world.execute(tx);
    assert(tx_removed.ok());
    assert(tx_world.revision() == WorldRevision{before_tx.value() + 1});
    assert(!tx_world.entities().contains(tx_avatar.value()));
    assert(tx_world.player_life().find(tx_avatar.value()) == nullptr);

    return 0;
}
