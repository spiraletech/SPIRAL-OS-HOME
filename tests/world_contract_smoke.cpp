#include <cassert>

#include "spiral/world/world_contract.hpp"

int main() {
    using namespace spiral::world;

    WorldSnapshot snapshot{};
    snapshot.world_id.value = "world:test";
    snapshot.coordinate_frame.origin_id.value = "origin:0";

    // Component-native entity: no transform is required.
    EntityRecord station{};
    station.id.value = "entity:radio:hollywood-hotline";
    station.kind = "radio_station";
    station.lifecycle = LifecycleState::Active;
    station.components.push_back(ComponentRecord{
        ComponentId{"component:station:identity"},
        "spiral.identity",
        "0.1",
        1,
        R"({"name":"HollywoodHotline"})"
    });
    snapshot.entities.push_back(station);

    // Sparse containment: World -> Region -> Volume -> Anchor is valid.
    snapshot.spatial_graph.nodes.push_back(SpatialNode{
        SpatialNodeId{"spatial:world"}, SpatialKind::World, {}, std::nullopt
    });
    snapshot.spatial_graph.nodes.push_back(SpatialNode{
        SpatialNodeId{"spatial:region"}, SpatialKind::Region, {}, SpatialNodeId{"spatial:world"}
    });
    snapshot.spatial_graph.nodes.push_back(SpatialNode{
        SpatialNodeId{"spatial:volume"}, SpatialKind::Volume, {}, SpatialNodeId{"spatial:region"}
    });
    snapshot.spatial_graph.nodes.push_back(SpatialNode{
        SpatialNodeId{"spatial:anchor"}, SpatialKind::Anchor, {}, SpatialNodeId{"spatial:volume"}
    });

    // HAKUI authors locomotion; HOME can commit the resulting delta without becoming author.
    WorldDelta delta{};
    delta.id.value = "delta:1";
    delta.transaction.value = "tx:1";
    delta.authority.value = "engine:hakui";
    delta.world_id = snapshot.world_id;
    delta.base_tick = 41;
    delta.commit_tick = 42;
    delta.sequence = 1;
    delta.changes.push_back(ComponentChange{
        EntityId{"entity:player:1"},
        "spiral.transform",
        7,
        8,
        ComponentOperationKind::Replace,
        R"({"position":[1.0,0.0,2.0]})"
    });

    assert(snapshot.entities.front().components.front().type == "spiral.identity");
    assert(snapshot.spatial_graph.nodes.size() == 4);
    assert(delta.authority.value == "engine:hakui");
    assert(delta.commit_tick > delta.base_tick);

    return 0;
}
