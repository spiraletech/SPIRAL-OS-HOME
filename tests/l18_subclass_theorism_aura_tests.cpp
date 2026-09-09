#include "home/versioned_world.hpp"

#include <algorithm>
#include <cassert>
#include <string>

namespace {

home::PlayerLifeState life_for(home::EntityId entity, std::uint64_t minute) {
    home::PlayerLifeState state{};
    state.entity = entity;
    state.stage = home::LifeStage::Adult;
    state.presence = home::LifePresence::Present;
    state.born_world_minute = 0;
    state.updated_world_minute = minute;
    state.sequence = 1;
    return state;
}

home::PlayerDynamicsState dynamics_for(
    home::EntityId entity,
    std::int32_t valence,
    std::int32_t arousal,
    std::uint64_t minute,
    std::uint64_t sequence) {
    home::PlayerDynamicsState state{};
    state.entity = entity;
    state.mood.valence = valence;
    state.mood.arousal = arousal;
    state.mood.band = home::derive_mood_band(valence);
    state.autonomy.mode = home::AutonomyMode::Disabled;
    state.updated_world_minute = minute;
    state.sequence = sequence;
    return state;
}

bool has_change(const std::vector<home::WorldChange>& changes, home::WorldChangeKind kind) {
    return std::find_if(changes.begin(), changes.end(), [&](const home::WorldChange& change) {
        return change.kind == kind;
    }) != changes.end();
}

} // namespace

int main() {
    using namespace home;

    VersionedWorld world{WorldId{18}};
    assert(world.advance_time(30'000).ok()); // HOME minute 1
    const auto player = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "agnathos", "Agnathos"});
    assert(player.ok());
    assert(world.set_player_life_state(life_for(player.value(), 1)).ok());

    // Aura does not exist before lived subclass evidence exists.
    assert(!world.resolve_player_aura(player.value()).ok());
    assert(world.theorism().find_aura(player.value()) == nullptr);

    // First discovered subclass becomes canonical alongside a derived aura in one revision.
    SubclassAffinityState skater{};
    skater.player = player.value();
    skater.key = "street_skater";
    skater.evidence_points = 400;
    skater.affinity_permille = subclass_affinity_for_evidence(skater.evidence_points);
    skater.discovered_world_minute = 1;
    skater.updated_world_minute = 1;
    skater.sequence = 1;

    const WorldRevision before_skater = world.revision();
    assert(world.set_subclass_affinity(skater).ok());
    assert(world.revision() == before_skater.next().value());
    assert(world.theorism().subclasses_for(player.value()).size() == 1);
    const AuraState* first_aura = world.theorism().find_aura(player.value());
    assert(first_aura != nullptr);
    assert(first_aura->dominant_affinity == "street_skater");
    assert(first_aura->signature == "halalulu:street_skater:neutral");
    assert(first_aura->intensity_permille == 400);
    assert(first_aura->charge_milli == 0);
    assert(first_aura->coherence_permille == 1000);
    assert(first_aura->sequence == 1);
    assert(has_change(world.history().back().changes(), WorldChangeKind::SubclassAffinityStateChanged));
    assert(has_change(world.history().back().changes(), WorldChangeKind::AuraStateChanged));

    // Subclasses are nonexclusive. Equal-strength ties resolve deterministically by key.
    SubclassAffinityState explorer = skater;
    explorer.key = "explorer";
    explorer.sequence = 1;
    assert(world.set_subclass_affinity(explorer).ok());
    assert(world.theorism().subclasses_for(player.value()).size() == 2);
    const AuraState* tied_aura = world.theorism().find_aura(player.value());
    assert(tied_aura != nullptr);
    assert(tied_aura->dominant_affinity == "explorer");
    assert(tied_aura->sequence == 2);

    SubclassAffinityState steward = skater;
    steward.key = "steward";
    steward.evidence_points = 700;
    steward.affinity_permille = subclass_affinity_for_evidence(steward.evidence_points);
    steward.sequence = 1;
    assert(world.set_subclass_affinity(steward).ok());
    assert(world.theorism().subclasses_for(player.value()).size() == 3);
    assert(world.theorism().find_aura(player.value())->dominant_affinity == "steward");

    // Affinity cannot be forged independently from evidence.
    const WorldRevision before_bad = world.revision();
    SubclassAffinityState contradictory{};
    contradictory.player = player.value();
    contradictory.key = "cook";
    contradictory.evidence_points = 800;
    contradictory.affinity_permille = 799;
    contradictory.discovered_world_minute = 1;
    contradictory.updated_world_minute = 1;
    contradictory.sequence = 1;
    assert(!world.set_subclass_affinity(contradictory).ok());
    assert(world.revision() == before_bad);

    // Existing affinities require genuinely new evidence and immutable discovery chronology.
    SubclassAffinityState no_new_evidence = steward;
    no_new_evidence.sequence = 2;
    assert(!world.set_subclass_affinity(no_new_evidence).ok());
    assert(world.revision() == before_bad);

    SubclassAffinityState rewritten_discovery = steward;
    rewritten_discovery.evidence_points = 800;
    rewritten_discovery.affinity_permille = subclass_affinity_for_evidence(rewritten_discovery.evidence_points);
    rewritten_discovery.discovered_world_minute = 0;
    rewritten_discovery.sequence = 2;
    assert(!world.set_subclass_affinity(rewritten_discovery).ok());
    assert(world.revision() == before_bad);

    SubclassAffinityState future{};
    future.player = player.value();
    future.key = "scavenger";
    future.evidence_points = 100;
    future.affinity_permille = 100;
    future.discovered_world_minute = 2;
    future.updated_world_minute = 2;
    future.sequence = 1;
    assert(!world.set_subclass_affinity(future).ok());
    assert(world.revision() == before_bad);

    // L14 mood/arousal changes refresh semantic AuraState in the same world revision.
    const AuraState aura_before_dynamics = *world.theorism().find_aura(player.value());
    const WorldRevision before_dynamics = world.revision();
    PlayerDynamicsState dynamics = dynamics_for(player.value(), 5'000, 8'000, 1, 1);
    assert(world.set_player_dynamics_state(dynamics).ok());
    assert(world.revision() == before_dynamics.next().value());
    const AuraState* energized = world.theorism().find_aura(player.value());
    assert(energized != nullptr);
    assert(energized->dominant_affinity == "steward");
    assert(energized->signature == "halalulu:steward:positive");
    assert(energized->intensity_permille == 900); // 700 affinity + 800 arousal-permille / 4
    assert(energized->charge_milli == 500);
    assert(energized->coherence_permille == 600);
    assert(energized->sequence == aura_before_dynamics.sequence + 1);
    assert(has_change(world.history().back().changes(), WorldChangeKind::PlayerDynamicsStateChanged));
    assert(has_change(world.history().back().changes(), WorldChangeKind::AuraStateChanged));

    const auto resolved = world.resolve_player_aura(player.value());
    assert(resolved.ok());
    assert(resolved.value().dominant_affinity == energized->dominant_affinity);
    assert(resolved.value().signature == energized->signature);
    assert(resolved.value().intensity_permille == energized->intensity_permille);
    assert(resolved.value().charge_milli == energized->charge_milli);
    assert(resolved.value().coherence_permille == energized->coherence_permille);

    // Adding evidence above the current dominant updates subclass truth deterministically.
    SubclassAffinityState explorer_growth = explorer;
    explorer_growth.evidence_points = 950;
    explorer_growth.affinity_permille = subclass_affinity_for_evidence(explorer_growth.evidence_points);
    explorer_growth.sequence = 2;
    assert(world.set_subclass_affinity(explorer_growth).ok());
    assert(world.theorism().find_aura(player.value())->dominant_affinity == "explorer");

    // Snapshot v12 preserves exact subclass + derived aura state.
    const WorldSnapshot saved = world.snapshot();
    assert(saved.subclasses.size() == 3);
    assert(saved.auras.size() == 1);
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    assert(encoded.value().rfind("HOME_SNAPSHOT 12", 0) == 0);
    assert(encoded.value().find("SUBCLASSES 3") != std::string::npos);
    assert(encoded.value().find("AURAS 1") != std::string::npos);

    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().subclasses == saved.subclasses);
    assert(decoded.value().auras == saved.auras);

    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    VersionedWorld restored = std::move(restored_result.value());
    assert(restored.revision() == world.revision());
    assert(restored.history().empty());
    assert(restored.theorism().subclass_snapshot() == saved.subclasses);
    assert(restored.theorism().aura_snapshot() == saved.auras);
    const auto reencoded = encode_snapshot(restored.snapshot());
    assert(reencoded.ok());
    assert(reencoded.value() == encoded.value());

    // v11 remains readable and naturally has no L18 theorism state.
    std::string legacy = encoded.value();
    const auto theorism_start = legacy.find("SUBCLASSES ");
    const auto entities_start = legacy.find("ENTITIES ", theorism_start);
    assert(theorism_start != std::string::npos && entities_start != std::string::npos);
    legacy.erase(theorism_start, entities_start - theorism_start);
    legacy.replace(0, std::string("HOME_SNAPSHOT 12").size(), "HOME_SNAPSHOT 11");
    const auto legacy_decoded = decode_snapshot(legacy);
    assert(legacy_decoded.ok());
    assert(legacy_decoded.value().subclasses.empty());
    assert(legacy_decoded.value().auras.empty());

    // Persisted aura cannot lie about HALALULU's deterministic semantic resolution.
    WorldSnapshot forged_aura = world.snapshot();
    forged_aura.auras.front().signature = "halalulu:forged:neutral";
    assert(!encode_snapshot(forged_aura).ok());

    WorldSnapshot missing_aura = world.snapshot();
    missing_aura.auras.clear();
    assert(!encode_snapshot(missing_aura).ok());

    WorldSnapshot orphan_subclass = world.snapshot();
    SubclassAffinityState orphan{};
    orphan.player = EntityId{999999};
    orphan.key = "orphan";
    orphan.evidence_points = 1;
    orphan.affinity_permille = 1;
    orphan.discovered_world_minute = 1;
    orphan.updated_world_minute = 1;
    orphan.sequence = 1;
    orphan_subclass.subclasses.push_back(orphan);
    assert(!encode_snapshot(orphan_subclass).ok());

    // Direct deletion purges all subclass + aura state in one world revision.
    const WorldRevision before_delete = world.revision();
    assert(world.remove_entity(player.value()).ok());
    assert(world.theorism().subclasses_for(player.value()).empty());
    assert(world.theorism().find_aura(player.value()) == nullptr);
    assert(world.revision() == before_delete.next().value());
    const auto& delete_changes = world.history().back().changes();
    assert(has_change(delete_changes, WorldChangeKind::SubclassAffinityStateChanged));
    assert(has_change(delete_changes, WorldChangeKind::AuraStateChanged));

    // Transactional deletion obeys exactly the same theorism cleanup invariant.
    VersionedWorld tx_world{WorldId{1802}};
    assert(tx_world.advance_time(30'000).ok());
    const auto tx_player = tx_world.create_entity(EntityCreateInfo{EntityKind::Avatar, "tx", "Tx"});
    assert(tx_player.ok());
    assert(tx_world.set_player_life_state(life_for(tx_player.value(), 1)).ok());
    SubclassAffinityState tx_subclass{};
    tx_subclass.player = tx_player.value();
    tx_subclass.key = "pilgrim";
    tx_subclass.evidence_points = 250;
    tx_subclass.affinity_permille = 250;
    tx_subclass.discovered_world_minute = 1;
    tx_subclass.updated_world_minute = 1;
    tx_subclass.sequence = 1;
    assert(tx_world.set_subclass_affinity(tx_subclass).ok());
    assert(tx_world.theorism().find_aura(tx_player.value()) != nullptr);

    WorldTransaction transaction{};
    transaction.id = WorldTransactionId{1802};
    transaction.expected_revision = tx_world.revision();
    transaction.authority = "home.l18.test";
    transaction.operations.push_back(TxRemoveEntity{tx_player.value()});
    assert(tx_world.execute(transaction).ok());
    assert(tx_world.theorism().subclasses_for(tx_player.value()).empty());
    assert(tx_world.theorism().find_aura(tx_player.value()) == nullptr);
    assert(has_change(tx_world.history().back().changes(), WorldChangeKind::SubclassAffinityStateChanged));
    assert(has_change(tx_world.history().back().changes(), WorldChangeKind::AuraStateChanged));

    return 0;
}
