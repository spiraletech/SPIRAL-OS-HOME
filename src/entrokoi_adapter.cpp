#include "home/entrokoi_adapter.hpp"

namespace home {

EntrokoiWorldView EntrokoiAdapter::snapshot() const {
    EntrokoiWorldView view{};
    view.world = world_.world();
    view.revision = world_.revision();
    view.calendar = world_.calendar();
    view.zones = world_.topology().snapshot_zones();
    view.connections = world_.topology().snapshot_connections();

    const auto records = world_.entities().snapshot();
    view.entities.reserve(records.size());
    for (const auto& record : records) {
        view.entities.push_back(EntrokoiEntityState{
            record.id,
            record.kind,
            record.archetype,
            record.display_name,
            record.transform,
            world_.topology().zone_of(record.id)
        });
    }

    return view;
}

} // namespace home
