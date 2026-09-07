#include "home/versioned_world.hpp"

#include <cassert>
#include <cstdio>

int main() {
    using namespace home;

    VersionedWorld world{WorldId{42}};

    ZoneCreateInfo child_info{};
    child_info.kind = ZoneKind::Parcel;
    child_info.key = "crystal_pier";
    child_info.display_name = "Crystal Pier";
    const auto child = world.create_zone(child_info);
    assert(child.ok());

    ZoneCreateInfo parent_info{};
    parent_info.kind = ZoneKind::District;
    parent_info.key = "pacific_beach";
    parent_info.display_name = "Pacific Beach";
    const auto parent = world.create_zone(parent_info);
    assert(parent.ok());

    const auto parented = world.set_zone_parent(child.value(), parent.value());
    assert(parented.ok());

    ZoneConnection connection{};
    connection.from = child.value();
    connection.to = parent.value();
    connection.bidirectional = true;
    connection.traversable = false;
    connection.tag = "maintenance_barrier";

    WorldTransaction connect_tx{};
    connect_tx.id = WorldTransactionId{1};
    connect_tx.expected_revision = world.revision();
    connect_tx.authority = "home.persistence.acceptance";
    connect_tx.operations = {TxConnectZones{connection}};
    const auto connected = world.execute(connect_tx);
    assert(connected.ok());
    assert(!world.topology().directly_traversable(child.value(), parent.value()));
    const auto canonical_connections = world.topology().snapshot_connections();
    assert(canonical_connections.size() == 1);
    assert(canonical_connections.front().tag == "maintenance_barrier");
    assert(!canonical_connections.front().traversable);

    EntityCreateInfo avatar_info{};
    avatar_info.kind = EntityKind::Avatar;
    avatar_info.archetype = "spiral.avatar";
    avatar_info.display_name = "Agnathos";
    const auto avatar = world.create_entity(avatar_info);
    assert(avatar.ok());

    const auto placed = world.place_entity(avatar.value(), child.value());
    assert(placed.ok());

    Transform moved{};
    moved.position = Vec3Mm{1234, 500, -900};
    moved.rotation = EulerMilliDegrees{0, 90000, 0};
    const auto moved_result = world.update_transform(avatar.value(), moved);
    assert(moved_result.ok());

    const WorldRevision saved_revision = world.revision();
    const WorldSnapshot captured = world.snapshot();
    assert(captured.world == WorldId{42});
    assert(captured.revision == saved_revision);
    assert(captured.entities.size() == 1);
    assert(captured.zones.size() == 2);
    assert(captured.connections.size() == 1);
    assert(captured.connections.front().tag == "maintenance_barrier");
    assert(!captured.connections.front().traversable);
    assert(captured.placements.size() == 1);

    const auto encoded = encode_snapshot(captured);
    assert(encoded.ok());
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().revision == saved_revision);
    assert(decoded.value().connections.size() == 1);
    assert(decoded.value().connections.front().tag == "maintenance_barrier");
    assert(!decoded.value().connections.front().traversable);

    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    const VersionedWorld& restored = restored_result.value();
    assert(restored.world() == WorldId{42});
    assert(restored.revision() == saved_revision);
    assert(restored.history().empty());
    assert(restored.entities().contains(avatar.value()));
    assert(restored.entities().find(avatar.value())->transform == moved);
    assert(restored.topology().find(child.value())->parent == parent.value());
    assert(restored.topology().zone_of(avatar.value()) == child.value());
    assert(!restored.topology().directly_traversable(child.value(), parent.value()));
    const auto restored_connections = restored.topology().snapshot_connections();
    assert(restored_connections.size() == 1);
    assert(restored_connections.front().tag == "maintenance_barrier");
    assert(!restored_connections.front().traversable);

    VersionedWorld resumed = std::move(VersionedWorld::from_snapshot(decoded.value())).value();
    EntityCreateInfo item_info{};
    item_info.kind = EntityKind::Item;
    item_info.archetype = "spiral.item";
    const auto next_entity = resumed.create_entity(item_info);
    assert(next_entity.ok());
    assert(next_entity.value() == EntityId{2});
    assert(resumed.revision() == *saved_revision.next());
    assert(resumed.history().size() == 1);

    const char* path = "home_l6_roundtrip.snapshot";
    const auto saved = save_snapshot_file(captured, path);
    assert(saved.ok());
    const auto loaded = load_snapshot_file(path);
    assert(loaded.ok());
    assert(loaded.value().world == captured.world);
    assert(loaded.value().revision == captured.revision);
    assert(loaded.value().entities.size() == captured.entities.size());
    assert(loaded.value().connections.front().tag == "maintenance_barrier");
    assert(!loaded.value().connections.front().traversable);
    std::remove(path);

    const auto malformed = decode_snapshot("HOME_SNAPSHOT 999\n");
    assert(!malformed.ok());
    assert(malformed.error().code == ErrorCode::SerializationError);

    return 0;
}
